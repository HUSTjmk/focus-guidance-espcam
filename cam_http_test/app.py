from fastapi import FastAPI, File, UploadFile, Form
from fastapi.responses import JSONResponse
from starlette.background import BackgroundTasks
import aiofiles
import matplotlib.pyplot as plt
from PIL import Image
import socket
import uvicorn
import logging
import os

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI()

@app.post("/upload/")
async def upload_image(
    question: str = Form(...),
    file: UploadFile = File(...),
    background_tasks: BackgroundTasks = BackgroundTasks(),
):
    if file.content_type != "image/jpeg":
        return JSONResponse(content={"success": False, "message": "Only JPEG images are supported."}, status_code=400)

    # 保存文件
    file_path = "uploaded_image.jpg"
    async with aiofiles.open(file_path, "wb") as out_file:
        content = await file.read()
        await out_file.write(content)

    # 定义异步任务处理图片
    def process_image():
        img = Image.open(file_path)
        plt.imshow(img)
        plt.axis("off")
        plt.title("Uploaded Image")
        plt.show()

    background_tasks.add_task(process_image)

    logger.info(f"Received question: {question}")
    return JSONResponse(content={"success": True, "message": "question and jpeg received"}, status_code=200)

def get_local_ip():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"

if __name__ == "__main__":
    local_ip = get_local_ip()
    logger.info(f"Server running at: http://{local_ip}:8000")
    uvicorn.run(app, host="0.0.0.0", port=8000)