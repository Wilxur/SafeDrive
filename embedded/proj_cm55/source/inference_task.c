 /******************************************************************************
* File Name: inference_task.c
*
* Description: This file contains API calls to inference task.
*
*
********************************************************************************
* (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header File
*******************************************************************************/
#include <math.h>
#include <inttypes.h>
#include <stdint.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
/* FreeRTOS header file */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "FreeRTOSConfig.h"
#include "stdlib.h"
#include "model.h"
#include "ifx_image_utils.h"
#include "lcd_task.h"
#include "inference_task.h"
#include "ifx_time_utils.h"
#include "mtb_ml_utils.h"
#include "mtb_ml_common.h"
#include "mtb_ml.h"
#include "shared_dms.h"
#include "voice_player.h"
/* 将大数组放入 CM55_PSRAM 外部内存，避免占用有限的内部 SRAM */
__attribute__((section(".cy_xip")))
__attribute__((aligned(16))) float image_buf_float[IMAI_DATA_IN_COUNT];


/* 6 类名称，顺序严格对应模型输出的 class_id（0~5）
 * 0:steering_wheel, 1:hand, 2:yawn, 3:seatbelt, 4:no_seatbelt, 5:phone */
static const char* dms_class_names[NUM_CLASSES] = {
    "steering_wheel",   // 0
    "hand",             // 1
    "yawn",             // 2
    "seatbelt",         // 3
    "no_seatbelt",      // 4
    "phone",            // 5
};

/*******************************************************************************
 * Global Variable
 *******************************************************************************/
/* Performance measure: Buffer wait time */ 
float prep_wait_buf; 
/* Performance measure: YUV422 to BGR565 conversion time */              
float prep_YUV422_to_bgr565; 
/* Performance measure: BGR565 to display time */      
float prep_bgr565_to_disp;   
/* Performance measure: RGB565 to RGB888 conversion time */      
float prep_RGB565_to_RGB888; 
      
#ifdef USE_DVP_CAM
extern bool frame_ready;
#endif

__attribute__((section(".cy_socmem_data")))
/* Final output variable for object detection */
__attribute__((aligned(16))) prediction_od_t prediction; 
__attribute__((section(".cy_xip")))
__attribute__((aligned(16))) float data_out[IMAI_DATA_OUT_COUNT];
/* Inference execution time */
volatile float inference_time = 0;
volatile float inference_fps = 0;
/* The best class out of all i.e. the class with max value out of all classes */
float max_class_val = 0;

/* Shared memory frame counter */
static uint32_t dms_frame_id = 0;

/* ========== 帧差分唤醒参数 (Frame-Difference Wake-Up) ========== */
#define FD_GRID_SIZE          40        /* 下采样网格 40×40 = 1600 个灰度单元 */
#define FD_CELL_PX             (IMAGE_WIDTH / FD_GRID_SIZE)  /* 224/40 = 5 */
#define FD_MOTION_THRESHOLD    6.0f      /* 平均像素差异阈值 (0-255) */
#define FD_WAKE_FRAMES          3         /* 连续检测到运动才唤醒，防误触发 */
#define FD_IDLE_FRAMES         30         /* 连续静止才休眠，防频繁切换 */

static uint8_t  fd_prev_gray[FD_GRID_SIZE * FD_GRID_SIZE];  /* 上一帧灰度图 */
static bool     fd_prev_valid = false;    /* 是否已存储上一帧数据 */
static bool     cm55_active_state = false; /* 当前状态: false=IDLE, true=ACTIVE */
static uint32_t motion_frame_cnt = 0;     /* 连续运动帧计数 */
static uint32_t still_frame_cnt  = 0;     /* 连续静止帧计数 */
static uint32_t skip_first_frames = 5;    /* 启动时跳过前 N 帧, 避免摄像头噪声误触发 */
static bool     had_detection = false;    /* 当前 ACTIVE 周期是否检测到过目标 */

/*******************************************************************************
 * Function Name: get_image
 *
 * Description:
 *   Retrieves the latest image by calling the draw function.
 *
 * Input Arguments:
 *   None
 *
 * Return Value:
 *   uint8_t* - Pointer to the image buffer
 *
 *******************************************************************************/
static uint8_t * get_image()
{
    return draw();
}

/*******************************************************************************
* Function Name: get_best_class
*******************************************************************************
*
* Summary:
*  The function calculates the best class out of all available classes.
*
* Parameters:
*  cls  - Pointer to the class array.
*  size - number of classes for the model i.e. size of class array.
*  max_cls_val - pointer to store the the best class out of all i.e. 
*                the max of all classes
*
* Return:
*  max_index - Returns the idex of the best class out of all i.e. 
*              the max of all classes
*
*******************************************************************************/
int8_t get_best_class(const float *cls, size_t size, float *max_cls_val) {
    if (cls == NULL || size == 0) {
    /* Array is empty */
        return -1;
    }

    int8_t max_index = 0;
    float max_value = cls[0];

    for (int8_t i = 1; i < size; i++) {
        if (cls[i] > max_value) {
            max_value = cls[i];
            max_index = i;
        }
    }

    *max_cls_val = max_value;
    return max_index;
}

/*******************************************************************************
 * Function Name: cm55_inference_task
 *
 * Description:
 *   Main task for running object detection inference on the CM55 core. Initializes
 *   the object detection model, preprocesses input images, performs inference, and
 *   postprocesses results. Updates performance metrics and signals the graphics
 *   display semaphore.
 *
 * Input Arguments:
 *   void *arg - Task argument (not used)
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
/* ========== NMS 辅助函数 ========== */
static float iou(float x1, float y1, float w1, float h1,
                 float x2, float y2, float w2, float h2)
{
    float xmin1 = x1 - w1 * 0.5f;
    float ymin1 = y1 - h1 * 0.5f;
    float xmax1 = x1 + w1 * 0.5f;
    float ymax1 = y1 + h1 * 0.5f;

    float xmin2 = x2 - w2 * 0.5f;
    float ymin2 = y2 - h2 * 0.5f;
    float xmax2 = x2 + w2 * 0.5f;
    float ymax2 = y2 + h2 * 0.5f;

    float inter_xmin = (xmin1 > xmin2) ? xmin1 : xmin2;
    float inter_ymin = (ymin1 > ymin2) ? ymin1 : ymin2;
    float inter_xmax = (xmax1 < xmax2) ? xmax1 : xmax2;
    float inter_ymax = (ymax1 < ymax2) ? ymax1 : ymax2;

    float inter_w = (inter_xmax > inter_xmin) ? (inter_xmax - inter_xmin) : 0.0f;
    float inter_h = (inter_ymax > inter_ymin) ? (inter_ymax - inter_ymin) : 0.0f;
    float inter_area = inter_w * inter_h;

    float area1 = w1 * h1;
    float area2 = w2 * h2;
    float union_area = area1 + area2 - inter_area;

    return (union_area > 0.0f) ? (inter_area / union_area) : 0.0f;
}

/* 简单排序：按置信度降序 */
static void sort_by_conf(float *conf, int *idx, int n)
{
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (conf[idx[j]] > conf[idx[i]]) {
                int tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
            }
        }
    }
}
/* ========== NMS 结束 ========== */

/* Determine alarm_level from the highest-priority alarm class present.
 * Returns 0 if no alarm classes detected.
 * Priority: hands_off > no_seatbelt > phone > yawn */
static uint8_t determine_alarm_level(uint8_t *class_ids, int count, int has_hands_off)
{
    if (has_hands_off) return ALARM_HANDS_OFF;
    for (int i = 0; i < count; i++) {
        switch (class_ids[i]) {
            case 4:  return ALARM_NO_SEATBELT;   /* no_seatbelt */
            case 5:  return ALARM_PHONE;         /* phone */
            case 2:  return ALARM_YAWN;          /* yawn */
            default: break;
        }
    }
    return ALARM_NONE;
}

/* ========== 帧差分辅助函数 ========== */

/*******************************************************************************
 * Function Name: downsample_gray
 *
 * Description:
 *   将 320×320 RGB888 图像下采样为 40×40 灰度网格。
 *   每个网格单元取 8×8 像素的平均灰度，灰度公式 (R + 2G + B) / 4。
 *
 * Input Arguments:
 *   rgb888  - 320×320×3 的 RGB888 图像缓冲区
 *   gray_out - 输出的 40×40（1600 字节）灰度数组
 *
 *******************************************************************************/
static void downsample_gray(const uint8_t *rgb888, uint8_t *gray_out)
{
    for (int gy = 0; gy < FD_GRID_SIZE; gy++) {
        for (int gx = 0; gx < FD_GRID_SIZE; gx++) {
            uint32_t sum = 0;
            int count = 0;

            /* 累加 8×8 像素块的灰度值 */
            for (int dy = 0; dy < FD_CELL_PX; dy++) {
                for (int dx = 0; dx < FD_CELL_PX; dx++) {
                    int px = gx * FD_CELL_PX + dx;
                    int py = gy * FD_CELL_PX + dy;
                    int idx = (py * IMAGE_WIDTH + px) * 3;

                    uint8_t r = rgb888[idx + 0];
                    uint8_t g = rgb888[idx + 1];
                    uint8_t b = rgb888[idx + 2];

                    /* 灰度公式 (R + 2G + B) / 4 —— 亮度近似 */
                    sum += (r + 2 * g + b) >> 2;
                    count++;
                }
            }

            gray_out[gy * FD_GRID_SIZE + gx] = (uint8_t)(sum / count);
        }
    }
}

/*******************************************************************************
 * Function Name: frame_diff
 *
 * Description:
 *   计算当前帧与上一帧之间的平均像素差异（0-255范围）。
 *   若上一帧数据无效（首帧），返回 0.0f 并存储当前帧。
 *
 * Input Arguments:
 *   cur_gray  - 当前帧的 40×40 灰度数组
 *
 * Return Value:
 *   float - 两帧之间的平均像素差异
 *
 *******************************************************************************/
static float frame_diff(const uint8_t *cur_gray)
{
    if (!fd_prev_valid) {
        /* 首帧：存储并返回 0 */
        memcpy(fd_prev_gray, cur_gray, FD_GRID_SIZE * FD_GRID_SIZE);
        fd_prev_valid = true;
        return 0.0f;
    }

    uint32_t diff_sum = 0;
    int total = FD_GRID_SIZE * FD_GRID_SIZE;

    for (int i = 0; i < total; i++) {
        int d = (int)cur_gray[i] - (int)fd_prev_gray[i];
        if (d < 0) d = -d;
        diff_sum += (uint32_t)d;
    }

    /* 更新上一帧 */
    memcpy(fd_prev_gray, cur_gray, FD_GRID_SIZE * FD_GRID_SIZE);

    return (float)diff_sum / (float)total;
}

/* ========== 帧差分辅助函数结束 ========== */

void cm55_inference_task(void *arg)
{
    int result = IMAI_init();
    IMAI_api_def *api_def = IMAI_api();

    /* ========== 调试：打印模型元数据（确认正常后可删除） ========== */
    printf("\r\n=== Model API Debug ===\r\n");
    printf("API ver: %lu, func_count: %ld\r\n", 
           (unsigned long)api_def->api_ver, (long)api_def->func_count);
    
    for (int f = 0; f < api_def->func_count; f++) {
        IMAI_func_def *func = &api_def->func_list[f];
        printf("  func[%d]: name=%s, desc=%s, param_count=%ld\r\n", 
               f,
               func->name ? func->name : "NULL",
               func->description ? func->description : "NULL",
               (long)func->param_count);
        
        for (int p = 0; p < func->param_count; p++) {
            IMAI_param_def *param = &func->param_list[p];
            printf("    param[%d]: name=%s, attrib=%d, rank=%ld, count=%ld, type_id=%ld\r\n",
                   p,
                   param->name ? param->name : "NULL",
                   param->attrib,
                   (long)param->rank,
                   (long)param->count,
                   (long)param->type_id);
            
            for (int s = 0; s < param->rank; s++) {
                printf("      shape[%d]: name=%s, size=%ld\r\n",
                       s,
                       param->shape[s].name ? param->shape[s].name : "NULL",
                       (long)param->shape[s].size);
            }
        }
    }
    printf("=====================\r\n\r\n");
    /* ========== 调试结束 ========== */

    float class[MAX_PREDICTIONS][NUM_CLASSES] = {{0.0}};
    volatile float _time_start_prev = 0;
    (void)_time_start_prev;

    /* 自适应新旧模型的 shape 顺序和内存布局
     * 旧模型: shape {13,6300}, d8[6300,13] box-major, flag有
     * 新模型: shape {1029,10}, d8[10,1029] attr-major, flag无 */
    int dim0 = api_def->func_list[0].param_list[1].shape[0].size;
    int dim1 = api_def->func_list[0].param_list[1].shape[1].size;
    int num_attributes   = (dim0 < dim1) ? dim0 : dim1;  // 属性数 (取小的)
    int max_predictions  = (dim0 < dim1) ? dim1 : dim0;  // 候选框数 (取大的)
    bool attr_major = (dim0 > dim1);   // shape[0]大表示属性是第二维, attr-major
    int class_offset = attr_major ? 4 : 5;  // 有flag时offset=5, 无flag时=4

    int actual_max_predictions = max_predictions;  /* 完整遍历 */

    int expected_classes = attr_major ? (num_attributes - 4) : (num_attributes - 5);
    if(NUM_CLASSES != expected_classes)
    {
        printf("The number of classes configured is wrong. Please update the NUM_CLASSES "
               "to %d.\r\n", expected_classes);
        CY_ASSERT(0);
    }

    if(result != 0)
        printf("Failed to initialize the model\r\n");

    while (true)
    {
#ifdef USE_DVP_CAM
        if (frame_ready == true)
#endif
        {
            volatile float _time_start = ifx_time_get_ms_f();
#ifdef USE_DVP_CAM
            frame_ready = false;
#endif
            /* Get the latest input image to the input image buffer. */
            uint8_t *image_buf_uint8 = get_image();
            cy_rtos_delay_milliseconds(1);

            /* ========== 帧差分检测 ========== */
            uint8_t fd_cur_gray[FD_GRID_SIZE * FD_GRID_SIZE];
            downsample_gray(image_buf_uint8, fd_cur_gray);
            float avg_diff = frame_diff(fd_cur_gray);

            /* 修复: 启动时跳过前 N 帧, 摄像头初始化阶段帧数据不稳定,
             * 避免全零 buffer 或噪声帧导致误唤醒和虚假检测 */
            if (skip_first_frames > 0) {
                skip_first_frames--;
                if (skip_first_frames == 0) {
                    printf("[FD] Warmup complete, frame diff active\r\n");
                }
                /* 仅更新共享内存状态, 不执行推理和状态机 */
                volatile dms_shared_mem_t *shm = DMS_SHARED_MEM_ADDR;
                DMS_BARRIER();
                shm->cm55_active = false;
                shm->idle_frame_count = 0;
                DMS_BARRIER();

                /* ★ 关键: 必须释放信号量, 否则 LCD task 阻塞在 semaphore_get */
                prediction.count = 0;
                cy_rtos_semaphore_set(&model_semaphore);

                continue;
            }

            /* 状态机：根据运动/静止计数切换 IDLE/ACTIVE */
            if (avg_diff > FD_MOTION_THRESHOLD) {
                motion_frame_cnt++;
                still_frame_cnt = 0;
            } else {
                still_frame_cnt++;
                motion_frame_cnt = 0;
            }

            if (!cm55_active_state) {
                /* IDLE 模式：连续 FD_WAKE_FRAMES 帧有运动 → 唤醒 */
                if (motion_frame_cnt >= FD_WAKE_FRAMES) {
                    cm55_active_state = true;
                    motion_frame_cnt = 0;
                    still_frame_cnt = 0;
                    had_detection = false;  /* 新 ACTIVE 周期, 重置检测标记 */
                    printf("[FD] WAKEUP: motion detected (avg_diff=%.1f)\r\n", avg_diff);
                }
            } else {
                /* ACTIVE 模式：连续静止 + 无检测目标 → 休眠
                 * 如果画面内有目标物品(即使静止), 保持 ACTIVE 继续监控 */
                if (still_frame_cnt >= FD_IDLE_FRAMES && !had_detection) {
                    cm55_active_state = false;
                    motion_frame_cnt = 0;
                    still_frame_cnt = 0;
                    printf("[FD] SLEEP: scene still + no target for %d frames\r\n", FD_IDLE_FRAMES);
                }
            }

            /* 写入 cm55_active 状态到共享内存（CM33_ns 据此决定是否轮询） */
            {
                volatile dms_shared_mem_t *shm = DMS_SHARED_MEM_ADDR;
                DMS_BARRIER();
                shm->cm55_active = cm55_active_state;
                shm->idle_frame_count = still_frame_cnt;
                DMS_BARRIER();
            }
            /* ========== 帧差分检测结束 ========== */

            /* 仅 ACTIVE 模式执行 YOLO 推理；IDLE 模式跳过以节省功耗 */
            volatile float _time_object_det = 0.0f;

            if (cm55_active_state) {

/* BGR888 uint8 -> float（和 PC 端 detect.py 完全一致：/255） */
for (int i = 0; i < IMAI_DATA_IN_COUNT; i++)
{
    image_buf_float[i] = (float)image_buf_uint8[i] / 255.0f;
}
            _time_object_det = ifx_time_get_ms_f();
            IMAI_compute(image_buf_float, data_out);

            /* ========== NMS 后处理 ========== */
            #define MAX_RAW_DETECTIONS 50
            float det_x[MAX_RAW_DETECTIONS];
            float det_y[MAX_RAW_DETECTIONS];
            float det_w[MAX_RAW_DETECTIONS];
            float det_h[MAX_RAW_DETECTIONS];
            float det_conf[MAX_RAW_DETECTIONS];
            int   det_class[MAX_RAW_DETECTIONS];
            int   raw_count = 0;

            for (int r = 0; r < actual_max_predictions; r++) {
                float cx, cy, w, h, flag;

                if (attr_major) {
                    cx = data_out[0 * max_predictions + r];
                    cy = data_out[1 * max_predictions + r];
                    w  = data_out[2 * max_predictions + r];
                    h  = data_out[3 * max_predictions + r];
                } else {
                    flag = data_out[r * num_attributes + 4];
                    if (flag < 0.10f) continue;
                    cx = data_out[r * num_attributes + 0];
                    cy = data_out[r * num_attributes + 1];
                    w  = data_out[r * num_attributes + 2];
                    h  = data_out[r * num_attributes + 3];
                }

                if (w <= 0.0f || h <= 0.0f || cx < 0.0f || cy < 0.0f || cx > 1.0f || cy > 1.0f) {
                    continue;
                }

                float best_cls_score = 0.0f;
                int best_cls = -1;
                for (int cls_no = 0; cls_no < NUM_CLASSES; cls_no++) {
                    float s;
                    if (attr_major)
                        s = data_out[(class_offset + cls_no) * max_predictions + r];
                    else
                        s = data_out[r * num_attributes + class_offset + cls_no];
                    if (s > best_cls_score) {
                        best_cls_score = s;
                        best_cls = cls_no;
                    }
                }

                if (!attr_major) {
                    best_cls_score *= flag;
                }

                /* steering_wheel(4)和hand(5)少样本, 阈值降到0.15 */
                /* wheel(0)/hand(1)/seatbelt(3)/no_seatbelt(4) 少样本, 阈值0.15 */
                float min_conf = (best_cls == 0 || best_cls == 1 || best_cls == 3 || best_cls == 4) ? 0.15f : 0.25f;
                if (best_cls < 0 || best_cls_score < min_conf) continue;


                /* 过滤过小的框（模型噪声），至少 15x15 像素  */
                if (w * IMAGE_WIDTH < 15.0f || h * IMAGE_HEIGHT < 15.0f) continue;

                if (raw_count >= MAX_RAW_DETECTIONS) break;

                /* 存入候选框数组，稍后做 NMS */
                det_x[raw_count] = cx;
                det_y[raw_count] = cy;
                det_w[raw_count] = w;
                det_h[raw_count] = h;
                det_conf[raw_count] = best_cls_score;
                det_class[raw_count] = best_cls;
                raw_count++;
            }

            /* ========== NMS 非极大值抑制 ========== */
            int order[MAX_RAW_DETECTIONS];
            for (int i = 0; i < raw_count; i++) order[i] = i;
            for (int i = 0; i < raw_count - 1; i++) {
                for (int j = i + 1; j < raw_count; j++) {
                    if (det_conf[order[j]] > det_conf[order[i]]) {
                        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
                    }
                }
            }

            /* keep[] 语义: 0=保留, 1=被NMS抑制丢弃
             * 修复: 原代码 keep[idx_i]=1 和 keep[idx_j]=1 均标记为1,
             * 而写入循环 !keep[k] 又将 keep[]==1 理解为"保留",
             * 导致选中框和被抑制框全部输出, NMS 完全无效。
             */
            int keep[MAX_RAW_DETECTIONS] = {0};
            int final_count = 0;

            for (int i = 0; i < raw_count && final_count < MAX_PREDICTIONS; i++) {
                int idx_i = order[i];
                if (keep[idx_i]) continue;   /* 已被抑制, 跳过 */

                /* 不设置 keep[idx_i] — 保留此框 (keep[]=0 表示保留) */
                final_count++;

                for (int j = i + 1; j < raw_count; j++) {
                    int idx_j = order[j];
                    if (keep[idx_j]) continue;
                    if (det_class[idx_i] != det_class[idx_j]) continue;

                    float iou_val = iou(det_x[idx_i], det_y[idx_i], det_w[idx_i], det_h[idx_i],
                                        det_x[idx_j], det_y[idx_j], det_w[idx_j], det_h[idx_j]);
                    if (iou_val > 0.35f) {
                        keep[idx_j] = 1;   /* 标记为"被抑制" */
                    }
                }
            }

            /* ========== 写入 prediction 结构体 ========== */
            int16_t *bbox_int16 = prediction.bbox_int16;
            float *conf = prediction.conf;
            uint8_t *class_id = prediction.class_id;
            int32_t actual_nr_predictions = 0;

            /* 类别短名称表（用于 LCD 显示，MAX_CLASS_LEN=10） */
            static const char* class_names[NUM_CLASSES] = {
                "wheel",      /* 0: steering_wheel */
                "hand",       /* 1: hand */
                "yawn",       /* 2: yawn */
                "belt",       /* 3: seatbelt */
                "no_belt",    /* 4: no_seatbelt */
                "phone",      /* 5: phone */
            };

            for (int i = 0; i < raw_count && actual_nr_predictions < MAX_PREDICTIONS; i++) {
                int k = order[i];            /* 按置信度降序遍历，与 NMS 保持一致 */
                if (keep[k]) continue;       /* keep[]=1 表示被 NMS 抑制, 跳过 */

                int cid = det_class[k];
                char *class_string = prediction.class_string[actual_nr_predictions];

                                float cx = det_x[k];      /* det_x[] 现在存的是中心点 cx */
                float cy = det_y[k];      /* det_y[] 现在存的是中心点 cy */
                float w  = det_w[k];
                float h  = det_h[k];

                /* 中心点 → 左上角（直接存 320×320 letterbox 空间像素坐标）
                 * 不做 clamp — 负数坐标由 LCD 端统一做 letterbox 补偿后裁剪 */
                *bbox_int16++ = (int16_t)(cx * IMAGE_WIDTH  - w * IMAGE_WIDTH  * 0.5f);
                *bbox_int16++ = (int16_t)(cy * IMAGE_HEIGHT - h * IMAGE_HEIGHT * 0.5f);
                *bbox_int16++ = (int16_t)(cx * IMAGE_WIDTH  + w * IMAGE_WIDTH  * 0.5f);   /* xmax */
                *bbox_int16++ = (int16_t)(cy * IMAGE_HEIGHT + h * IMAGE_HEIGHT * 0.5f);  /* ymax */
                class_id[actual_nr_predictions] = cid;
                conf[actual_nr_predictions] = det_conf[k];

                snprintf(class_string, MAX_CLASS_LEN, "%s", class_names[cid]);
                printf("  -> Box[%d]: class=%s, conf=%.0f%%\r\n", 
                       actual_nr_predictions, class_string, det_conf[k] * 100.0f);

                actual_nr_predictions++;
            }

            /* ========== hands_off 推断 (修复后: 仅当手不在方向盘上时触发) ========== */
            float wheel_x = -1, wheel_y = -1, wheel_w = -1, wheel_h = -1;
            int has_wheel = 0;
            float hand_x[5], hand_y[5], hand_w[5], hand_h[5];
            int hand_count = 0;

            for (int k = 0; k < raw_count; k++) {
                if (keep[k]) continue;   /* 跳过被 NMS 抑制的框 */
                int cid = det_class[k];
                if (cid == 0) {  /* steering_wheel */
                    wheel_x = det_x[k]; wheel_y = det_y[k];
                    wheel_w = det_w[k]; wheel_h = det_h[k];
                    has_wheel = 1;
                } else if (cid == 1 && hand_count < 5) {  /* hand */
                    hand_x[hand_count] = det_x[k]; hand_y[hand_count] = det_y[k];
                    hand_w[hand_count] = det_w[k]; hand_h[hand_count] = det_h[k];
                    hand_count++;
                }
            }

            /* 修复: 原代码 hands_on_wheel 初始化为 0 后从未赋值为 1,
             * 导致只要有方向盘检测就无条件触发 hands_off。
             * 现添加手与方向盘的重叠判断 (IoU > 0.1 即认为手在方向盘上) */
            int hands_on_wheel = 0;
            for (int hi = 0; hi < hand_count && !hands_on_wheel; hi++) {
                float h_iou = iou(wheel_x, wheel_y, wheel_w, wheel_h,
                                  hand_x[hi], hand_y[hi], hand_w[hi], hand_h[hi]);
                if (h_iou > 0.1f) {
                    hands_on_wheel = 1;
                }
            }

            if (has_wheel && !hands_on_wheel && actual_nr_predictions < MAX_PREDICTIONS) {
                char *class_string = prediction.class_string[actual_nr_predictions];
                
                /* wheel_x/wheel_y 现在存的是中心点 cx/cy，先转左上角 */
                float wheel_xmin = wheel_x - wheel_w * 0.5f;
                float wheel_ymin = wheel_y - wheel_h * 0.5f;

                float x = wheel_xmin;
                float y = wheel_ymin + wheel_h * 0.3f;
                float w = wheel_w;
                float h = wheel_h * 0.4f;

                *bbox_int16++ = (int16_t)(x * IMAGE_WIDTH);
                *bbox_int16++ = (int16_t)(y * IMAGE_HEIGHT);
                *bbox_int16++ = (int16_t)((x + w) * IMAGE_WIDTH);
                *bbox_int16++ = (int16_t)((y + h) * IMAGE_HEIGHT);

                class_id[actual_nr_predictions] = 255;  /* hands_off 专用ID */
                conf[actual_nr_predictions] = 0.85f;
                snprintf(class_string, MAX_CLASS_LEN, "hands_off");
                printf("  -> Box[%d]: class=hands_off (inferred)\r\n", actual_nr_predictions);
                actual_nr_predictions++;
            }
            /* ========== hands_off 推断结束 ========== */

            prediction.count = actual_nr_predictions;

            /* ========== Write to shared memory (for CM33_ns / backend) ========== */
            {
                volatile dms_shared_mem_t *shm = DMS_SHARED_MEM_ADDR;
                int has_hands_off = 0;

                shm->frame_id = dms_frame_id++;

                int nboxes = (actual_nr_predictions < DMS_MAX_BOXES)
                             ? actual_nr_predictions : DMS_MAX_BOXES;
                shm->box_count = (uint8_t)nboxes;
                had_detection = (nboxes > 0);  /* 当前帧是否有目标 */

                uint8_t alarm_class_ids[DMS_MAX_BOXES];
                for (int i = 0; i < nboxes; i++) {
                    uint8_t cid = prediction.class_id[i];
                    shm->boxes[i].class_id   = cid;
                    shm->boxes[i].confidence = (uint16_t)(prediction.conf[i] * 10000.0f);
                    shm->boxes[i].x = prediction.bbox_int16[i * 4 + 0];
                    shm->boxes[i].y = prediction.bbox_int16[i * 4 + 1];
                    shm->boxes[i].w = prediction.bbox_int16[i * 4 + 2]
                                    - prediction.bbox_int16[i * 4 + 0];
                    shm->boxes[i].h = prediction.bbox_int16[i * 4 + 3]
                                    - prediction.bbox_int16[i * 4 + 1];
                    alarm_class_ids[i] = cid;

                    if (strcmp(prediction.class_string[i], "hands_off") == 0) {
                        has_hands_off = 1;
                    }
                }

                shm->alarm_level = determine_alarm_level(alarm_class_ids, nboxes,
                                                         has_hands_off);

                /* Voice alert: triggers on every detection frame.
                 * Waits for current playback to finish before starting next.
                 * Result: voice loops continuously while alarm persists. */
                {
                    if (shm->alarm_level > 0 && !voice_player_is_busy()) {
                        switch (shm->alarm_level) {
                            case ALARM_PHONE:
                                voice_player_play(VOICE_PHONE);
                                break;
                            case ALARM_YAWN:
                                voice_player_play(VOICE_YAWN);
                                break;
                            case ALARM_NO_SEATBELT:
                                voice_player_play(VOICE_SEATBELT);
                                break;
                            case ALARM_HANDS_OFF:
                                voice_player_play(VOICE_HANDSOFF);
                                break;
                            default: break;
                        }
                    }
                }

                if (nboxes > 0) {
                    int best_idx = 0;
                    for (int i = 1; i < nboxes; i++) {
                        if (shm->boxes[i].confidence > shm->boxes[best_idx].confidence) {
                            best_idx = i;
                        }
                    }
                    snprintf((char *)shm->top_class, sizeof(shm->top_class),
                             "%s", prediction.class_string[best_idx]);
                } else {
                    shm->top_class[0] = '\0';
                }

                /* 更新帧差分状态（ACTIVE 模式下必定为 true） */
                shm->cm55_active = true;
                shm->idle_frame_count = 0;

                DMS_BARRIER();
                shm->fresh = true;
                DMS_BARRIER();

                if (shm->alarm_level > 0) {
                    printf("[SHM] frame=%" PRIu32 " alarm=%u top=%s boxes=%u\r\n",
                           shm->frame_id, (unsigned)shm->alarm_level,
                           shm->top_class, (unsigned)shm->box_count);
                }
            }
            /* ========== End shared memory write ========== */

            } /* end if (cm55_active_state) */

            /* 性能计时：IDLE 模式下 inference_time 为 0 */
            if (cm55_active_state) {
                volatile float _time_end = ifx_time_get_ms_f();
                inference_time = _time_end - _time_object_det;
            } else {
                inference_time = 0.0f;
            }

            /* FPS 追踪: 每秒更新一次 */
            {
                static uint32_t fps_frame_count = 0;
                static float fps_last_time = 0;
                fps_frame_count++;
                float now = ifx_time_get_ms_f();
                if (now - fps_last_time >= 1000.0f) {
                    inference_fps = (float)fps_frame_count * 1000.0f / (now - fps_last_time);
                    fps_frame_count = 0;
                    fps_last_time = now;
                }
            }

            result = cy_rtos_semaphore_set(&model_semaphore);
            if (CY_RSLT_SUCCESS != result) {
                printf("\r\nModel Semaphore set failed\r\n");
            }

            _time_start_prev = _time_start;
        }
    }
}