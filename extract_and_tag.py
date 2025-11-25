import os
import sys
from pypdf import PdfReader
from docx import Document
from PIL import Image
import pytesseract
import requests
import json
import pandas as pd
from requests.exceptions import ConnectionError, Timeout
import subprocess
import time
import os

def start_ollama_llm(commond):
    """
    在后台启动 Ollama 服务。

    :param ollama_path: Ollama 可执行文件的路径或名称（如果它在PATH中）。
    :return: 启动的子进程对象 (Popen)
    """
    print("尝试启动 Ollama 服务...")
    
    # 根据操作系统设置启动命令和参数
    # 在 Windows 上，可能需要 'shell=True' 或直接使用完整的路径。
    # 这里使用简单的 ['ollama', 'serve'] 命令，假设 ollama 在 PATH 中。
    
    try:
        # 使用 Popen 在后台启动，并将标准输出/错误重定向，防止阻塞
        process = subprocess.Popen(
            commond,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            # 设置一个独立进程组，有助于管理
            start_new_session=True 
        )
        print(f"Ollama 服务已尝试启动，进程 ID: {process.pid}")
        
        # 给予 Ollama 几秒钟时间启动
        time.sleep(3) 
        
        # 检查进程是否仍然存活
        if process.poll() is None:
            print("Ollama 服务似乎已成功启动。请继续使用 API 检查其可用性。")
            return process
        else:
            # 如果进程立刻退出，检查是否有错误信息
            stdout, stderr = process.communicate()
            print(f"Ollama 启动失败，退出代码: {process.returncode}")
            print("Stderr:\n", stderr.decode())
            return None

    except FileNotFoundError:
        print(f"错误: 找不到 Ollama 可执行文件。请确保 '{ollama_path}' 在 PATH 中或提供完整路径。")
        return None
    except Exception as e:
        print(f"启动 Ollama 时发生意外错误: {e}")
        return None



def is_ollama_running(url="http://localhost:11434"):
    """
    通过尝试访问 Ollama 的 API 端点来检查它是否正在运行。
    """
    try:
        # 尝试访问 /api/tags 端点
        response = requests.get(f"{url}/api/tags", timeout=5)
        
        # 如果能成功连接并且收到任何有效的响应（例如 200），
        # 则说明 Ollama 正在运行。
        if response.status_code == 200:
            return True
        else:
            # 即使状态码不是 200，但连接成功，也说明服务可能已启动。
            # 例如，访问根路径可能会返回 404/405，但这仍表示端口是开放的。
            # 但是，访问 /api/tags 应该返回 200。
            # 为了更严格，我们只接受 200。
            return False

    except ConnectionError:
        # 如果连接失败（端口未开放），则 Ollama 未运行
        return False
    except Timeout:
        # 请求超时，通常也意味着服务未及时响应
        return False
    except Exception as e:
        # 处理其他可能的错误
        print(f"检查 Ollama 时发生意外错误: {e}")
        return False

def extract_text(file_path):
    ext = file_path.lower()

    # 文本文件
    if ext.endswith((".txt", ".csv", ".json", ".xml", ".log", ".md")):
        return open(file_path, "r", encoding="utf8", errors="ignore").read()

    # PDF
    if ext.endswith(".pdf"):
        reader = PdfReader(file_path)
        return "\n".join([page.extract_text() or "" for page in reader.pages])

    # Word
    if ext.endswith(".docx"):
        doc = Document(file_path)
        return "\n".join([p.text for p in doc.paragraphs])

    # 图片（OCR）
    if ext.endswith((".png", ".jpg", ".jpeg")):
        img = Image.open(file_path)
        return pytesseract.image_to_string(img, lang="chi_sim+eng")

    # Excel（可选）
    # if ext.endswith(".xlsx"):
    #     import pandas as pd
    #     df = pd.read_excel(file_path)
    #     return df.to_string()

    return None

def tag_text_qwen(text: str):
    prompt = f"""
请为下面的文件内容生成3个中文标签，标签要简短、概括核心内容，并用,分隔：
{text[:4000]}
"""

    payload = {
        "model": "qwen2.5:7b",
        "prompt": prompt
    }

    # Ollama 生成是多行 JSON，每行一个对象，因此 stream=True
    r = requests.post("http://localhost:11434/api/generate", json=payload, stream=True)

    full_resp = ""

    # 逐行读取模型生成的 response 字段
    for line in r.iter_lines():
        if not line:
            continue

        try:
            obj = json.loads(line.decode("utf-8"))
        except:
            continue  # 不是 JSON 的行跳过

        # Ollama 每个 JSON 都可能带 "response" 字段
        if "response" in obj:
            full_resp += obj["response"]

    # 清理格式，转换为列表
    tags = [tag.strip() for tag in full_resp.split(",") if tag.strip()]
    return tags

def tag_image_qwen(text: str):
    ext = file_path.lower()

    # 文本文件
    if not ext.endswith(".png", ".jpg", ".jpeg"):
        return ""

    prompt = f"""
请为下面的文件生成3个中文标签，标签要简短、概括核心内容，并用,分隔：
{text[:4000]}
"""

    payload = {
        "model": "qwen2.5vl:7b",
        "prompt": prompt
    }

    # Ollama 生成是多行 JSON，每行一个对象，因此 stream=True
    r = requests.post("http://localhost:11434/api/generate", json=payload, stream=True)

    full_resp = ""

    # 逐行读取模型生成的 response 字段
    for line in r.iter_lines():
        if not line:
            continue

        try:
            obj = json.loads(line.decode("utf-8"))
        except:
            continue  # 不是 JSON 的行跳过

        # Ollama 每个 JSON 都可能带 "response" 字段
        if "response" in obj:
            full_resp += obj["response"]

    # 清理格式，转换为列表
    tags = [tag.strip() for tag in full_resp.split(",") if tag.strip()]
    return tags

def main():
    if not is_ollama_running():
        start_ollama_llm('ollama run qwen2.5:7b')
        start_ollama_llm('ollama run qwen2.5vl:7b')
    
    fp = sys.argv[1]
    text = extract_text(fp)
    if text:
        tags = tag_text_qwen(text)
        print(json.dumps({"file": fp, "tags":tags}))
    else:
        tags = tag_image_qwen(fp)
        print(json.dumps({"file": fp, "tags":tags}))

if __name__ == "__main__":
    main()
