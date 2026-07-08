path = "D:/疲劳驾驶/frontend/src/views/admin/UserDetail.vue"
with open(path, encoding="utf-8") as f:
    c = f.read()

# Fix duplicate entries in categoryMap
old_map = """const categoryMap = {
  yawn: '\u6253\u54c8\u6b20', seatbelt: '\u7cfb\u5b89\u5168\u5e26', no_seatbelt: '\u672a\u7cfb\u5b89\u5168\u5e26',
  yawn: '\u6253\u54c8\u6b20', seatbelt: '\u7cfb\u5b89\u5168\u5e26', no_seatbelt: '\u672a\u7cfb\u5b89\u5168\u5e26', phone: '\u4f7f\u7528\u624b\u673a', steering_wheel: '\u65b9\u5411\u76d8', hand: '\u624b\u90e8',

}"""
new_map = """const categoryMap = {
  yawn: '\u6253\u54c8\u6b20', seatbelt: '\u7cfb\u5b89\u5168\u5e26', no_seatbelt: '\u672a\u7cfb\u5b89\u5168\u5e26',
  phone: '\u4f7f\u7528\u624b\u673a', steering_wheel: '\u65b9\u5411\u76d8', hand: '\u624b\u90e8',
}"""
c = c.replace(old_map, new_map)

# Fix duplicate in statItems
old_stat = """  { key: 'yawn_count', label: '\u6253\u54c8\u6b20', color: '#e6a23c' },
  { key: 'yawn_count', label: '\u6253\u54c8\u6b20', color: '#e6a23c' },"""
new_stat = """  { key: 'yawn_count', label: '\u6253\u54c8\u6b20', color: '#e6a23c' },"""
c = c.replace(old_stat, new_stat)

# Add company assignment to template: replace the static company line
old_company = "        <div class=\"info-row\"><span class=\"label\">\u7269\u6d41\u516c\u53f8</span><span>{{ userDetail.company_name || '-' }}</span></div>"
new_company = """        <div class="info-row"><span class="label">物流公司</span>
          <el-select v-model="editCompanyId" placeholder="选择公司" size="small" style="width:130px" clearable>
            <el-option v-for="c in companies" :key="c.id" :label="c.name" :value="c.id" />
          </el-select>
          <el-button size="small" type="primary" @click="handleUpdateUser" :loading="savingCompany" style="margin-left:6px">保存</el-button>
        </div>"""
c = c.replace(old_company, new_company)

# Also add license plate editing
old_plate = "<div class=\"info-row\"><span class=\"label\">\u8f66\u724c\u53f7</span><span>{{ userDetail.license_plate || '-' }}</span></div>"
new_plate = """        <div class="info-row"><span class="label">车牌号</span>
          <el-input v-model="editPlate" size="small" style="width:130px" placeholder="输入车牌号" />
          <el-button size="small" type="primary" @click="handleUpdateUser" :loading="savingCompany" style="margin-left:6px">保存</el-button>
        </div>"""
c = c.replace(old_plate, new_plate)

# Add script variables and functions
old_script_end = "onMounted(async () => {"
new_script_vars = """const companies = ref([])
const editCompanyId = ref(null)
const editPlate = ref('')
const savingCompany = ref(false)

async function loadCompanies() {
  try {
    const res = await adminAPI.getCompanies()
    companies.value = res.companies || []
  } catch (e) {}
}

async function handleUpdateUser() {
  savingCompany.value = true
  try {
    const data = {}
    if (editCompanyId.value !== null && editCompanyId.value !== undefined) data.company_id = editCompanyId.value || null
    if (editPlate.value !== undefined) data.license_plate = editPlate.value
    await adminAPI.updateUser(userId, data)
    ElMessage.success('保存成功')
    // Refresh user detail
    const res = await adminAPI.getUserDetail(userId)
    userDetail.value = res.user
  } catch (e) {}
  finally { savingCompany.value = false }
}

onMounted(async () => {"""
c = c.replace(old_script_end, new_script_vars)

# Add loadCompanies to onMounted
old_mount = "  loading.value = true\n  try {\n    const res = await adminAPI.getUserDetail(userId)"
new_mount = "  loading.value = true\n  try {\n    await loadCompanies()\n    const res = await adminAPI.getUserDetail(userId)"
c = c.replace(old_mount, new_mount)

# After setting userDetail, set the edit variables
old_set = "    userDetail.value = res.user\n    latestDetection.value = res.latest_detection"
new_set = """    userDetail.value = res.user
    editCompanyId.value = res.user?.company_id ?? null
    editPlate.value = res.user?.license_plate || ''
    latestDetection.value = res.latest_detection"""
c = c.replace(old_set, new_set)

with open(path, "w", encoding="utf-8") as f:
    f.write(c)
print("UserDetail.vue updated with company assignment")
