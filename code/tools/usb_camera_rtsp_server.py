#!/usr/bin/env python3
"""Publish a USB camera to an RTSP server for local phase-2 testing.

The script captures frames with OpenCV and pipes raw BGR frames to FFmpeg.
Run an RTSP server such as MediaMTX first, then this script publishes to it.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
import urllib.parse
from pathlib import Path


DEFAULT_FFMPEG = Path(
    r"D:\FFmpeg\ffmpeg-2025-05-19-git-c55d65ac0a-essentials_build\bin\ffmpeg.exe"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Publish a USB camera as an RTSP stream using OpenCV + FFmpeg."
    )
    parser.add_argument("--camera", type=int, default=0, help="USB camera index.")
    parser.add_argument("--width", type=int, default=1280, help="Capture width.")
    parser.add_argument("--height", type=int, default=720, help="Capture height.")
    parser.add_argument("--fps", type=int, default=25, help="Capture/output FPS.")
    parser.add_argument(
        "--url",
        default="rtsp://127.0.0.1:8554/usb_camera",
        help="RTSP publishing URL. Start an RTSP server such as MediaMTX first.",
    )
    parser.add_argument(
        "--ffmpeg-log",
        default="output/usb_camera_rtsp_ffmpeg.log",
        help="Path to write FFmpeg stderr logs.",
    )
    parser.add_argument(
        "--ffmpeg",
        default=str(DEFAULT_FFMPEG),
        help="Path to ffmpeg.exe. Falls back to PATH if this path is missing.",
    )
    parser.add_argument(
        "--bitrate",
        default="2500k",
        help="H.264 video bitrate, for example 2500k or 4M.",
    )
    parser.add_argument(
        "--show-preview",
        action="store_true",
        help="Show a local OpenCV preview window. Press q/ESC to quit.",
    )
    return parser.parse_args()


def resolve_ffmpeg(path_text: str) -> str:
    path = Path(path_text)
    if path.exists():
        return str(path)
    found = shutil.which("ffmpeg")
    if found:
        return found
    raise FileNotFoundError(
        f"ffmpeg not found. Expected {path_text!r} or ffmpeg in PATH."
    )


def open_camera(camera_index: int, width: int, height: int, fps: int):
    try:
        import cv2
    except ImportError as exc:
        raise RuntimeError(
            "Python package 'opencv-python' is required. Install it with: "
            "python -m pip install opencv-python"
        ) from exc

    cap = cv2.VideoCapture(camera_index, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap = cv2.VideoCapture(camera_index)
    if not cap.isOpened():
        raise RuntimeError(f"failed to open USB camera index {camera_index}")

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, fps)

    ok, frame = cap.read()
    if not ok or frame is None or frame.size == 0:
        cap.release()
        raise RuntimeError("camera opened but failed to read the first frame")
    return cv2, cap, frame


def build_ffmpeg_command(
    ffmpeg: str,
    width: int,
    height: int,
    fps: int,
    bitrate: str,
    url: str,
) -> list[str]:
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme != "rtsp" or not parsed.netloc:
        raise ValueError(f"invalid RTSP URL: {url}")

    return [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "info",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "bgr24",
        "-s",
        f"{width}x{height}",
        "-r",
        str(fps),
        "-i",
        "-",
        "-an",
        "-c:v",
        "libx264",
        "-preset",
        "veryfast",
        "-tune",
        "zerolatency",
        "-pix_fmt",
        "yuv420p",
        "-b:v",
        bitrate,
        "-f",
        "rtsp",
        "-rtsp_transport",
        "tcp",
        url,
    ]


def main() -> int:
    args = parse_args()
    ffmpeg = resolve_ffmpeg(args.ffmpeg)
    cv2, cap, first_frame = open_camera(args.camera, args.width, args.height, args.fps)

    actual_height, actual_width = first_frame.shape[:2]
    if actual_width != args.width or actual_height != args.height:
        print(
            f"camera returned {actual_width}x{actual_height}; using actual size",
            file=sys.stderr,
        )

    command = build_ffmpeg_command(
        ffmpeg=ffmpeg,
        width=actual_width,
        height=actual_height,
        fps=args.fps,
        bitrate=args.bitrate,
        url=args.url,
    )

    log_path = Path(args.ffmpeg_log)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_file = log_path.open("w", encoding="utf-8", errors="replace")

    print(f"Publishing USB camera {args.camera} to: {args.url}")
    print(f"FFmpeg log: {log_path}")
    print("Start an RTSP server first. Open this URL from VLC/ffplay or use it as phase2 input_rtsp_url.")
    print("Press Ctrl+C to stop.")

    process = subprocess.Popen(
        command,
        stdin=subprocess.PIPE,
        stdout=log_file,
        stderr=subprocess.STDOUT,
    )
    frame_interval = 1.0 / max(args.fps, 1)
    next_frame_time = time.perf_counter()

    try:
        frame = first_frame
        while True:
            if process.poll() is not None:
                raise RuntimeError(f"ffmpeg exited with code {process.returncode}")

            if frame.shape[1] != actual_width or frame.shape[0] != actual_height:
                frame = cv2.resize(frame, (actual_width, actual_height))

            process.stdin.write(frame.tobytes())

            if args.show_preview:
                cv2.imshow("usb camera rtsp source", frame)
                key = cv2.waitKey(1)
                if key in (27, ord("q"), ord("Q")):
                    break

            next_frame_time += frame_interval
            sleep_for = next_frame_time - time.perf_counter()
            if sleep_for > 0:
                time.sleep(sleep_for)

            ok, frame = cap.read()
            if not ok or frame is None or frame.size == 0:
                print("camera read failed, retrying...", file=sys.stderr)
                time.sleep(0.05)
                continue
    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        if args.show_preview:
            cv2.destroyAllWindows()
        if process.stdin:
            try:
                process.stdin.close()
            except OSError:
                pass
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
        log_file.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
