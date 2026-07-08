#!/usr/bin/env python3
"""
SafeDrive DMS Alarm Server
Receives HTTP POST from PSE84 CM33_ns core, logs alarm events.
Usage: python server.py [--port PORT] [--logfile PATH]
"""

import json
import logging
import sys
from datetime import datetime
from pathlib import Path

from flask import Flask, request, jsonify

app = Flask(__name__)

log = logging.getLogger("safedrive")
log.setLevel(logging.INFO)

console_handler = logging.StreamHandler(sys.stdout)
console_handler.setFormatter(logging.Formatter(
    "[%(asctime)s] %(levelname)s: %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
))
log.addHandler(console_handler)

log_filename = None

CLASS_NAMES = {
    0: "closed_eyes",
    1: "hand",
    2: "open_eyes",
    3: "yawn",
    4: "seatbelt",
    5: "no_seatbelt",
    6: "phone",
    7: "steering_wheel",
}

ALARM_NAMES = {
    0: "NONE",
    1: "CLOSED_EYES",
    2: "YAWN",
    3: "PHONE",
    4: "NO_SEATBELT",
    5: "HANDS_OFF",
}


@app.route("/api/dms/alarm", methods=["POST"])
def receive_alarm():
    """Receive and log a DMS alarm event."""
    try:
        data = request.get_json(force=True)
    except Exception as e:
        log.error("Failed to parse JSON: %s", e)
        return jsonify({"status": "error", "message": "Invalid JSON"}), 400

    frame_id = data.get("frame_id", 0)
    alarm_level = data.get("alarm_level", 0)
    top_class = data.get("top_class", "unknown")
    box_count = data.get("box_count", 0)
    boxes = data.get("boxes", [])

    alarm_name = ALARM_NAMES.get(alarm_level, "UNKNOWN(%d)" % alarm_level)

    log.info(
        "ALARM frame=%-6d level=%-2d (%s) top_class=%-12s boxes=%-2d",
        frame_id, alarm_level, alarm_name, top_class, box_count
    )

    for i, box in enumerate(boxes):
        cid = box.get("class_id", -1)
        cname = CLASS_NAMES.get(cid, "unknown(%d)" % cid)
        conf_val = box.get("conf", 0) / 100.0
        x, y, w, h = box.get("x", 0), box.get("y", 0), box.get("w", 0), box.get("h", 0)
        log.info(
            "  Box[%d]: class=%-14s conf=%.1f%%  rect=[%d,%d %dx%d]",
            i, cname, conf_val, x, y, w, h
        )

    if log_filename:
        log_record = {
            "timestamp": datetime.now().isoformat(),
            "frame_id": frame_id,
            "alarm_level": alarm_level,
            "alarm_name": alarm_name,
            "top_class": top_class,
            "box_count": box_count,
            "boxes": boxes,
        }
        with open(log_filename, "a", encoding="utf-8") as f:
            f.write(json.dumps(log_record) + "\n")

    return jsonify({"status": "ok", "alarm_name": alarm_name}), 200


@app.route("/health", methods=["GET"])
def health():
    """Health check endpoint."""
    return jsonify({"status": "ok"}), 200


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="SafeDrive DMS Alarm Server")
    parser.add_argument("--port", type=int, default=5000,
                        help="Server port (default: 5000)")
    parser.add_argument("--logfile", type=str, default=None,
                        help="Path to structured JSON alarm log file")
    parser.add_argument("--host", type=str, default="0.0.0.0",
                        help="Host to bind to (default: 0.0.0.0)")
    args = parser.parse_args()

    if args.logfile:
        log_filename = args.logfile
        Path(args.logfile).parent.mkdir(parents=True, exist_ok=True)
        log.info("Structured alarm logs will be written to: %s", args.logfile)

    log.info("Starting SafeDrive DMS Alarm Server on %s:%d", args.host, args.port)
    log.info("Endpoints: POST /api/dms/alarm  GET /health")
    app.run(host=args.host, port=args.port, debug=False)
