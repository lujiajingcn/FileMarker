import os
import sys
from pypdf import PdfReader
from docx import Document
from PIL import Image
import pytesseract
import requests
import json
import pandas as pd

def extract_text(file_path):
    ext = file_path.lower()

    # 文本文件
    if ext.endswith(".txt") or ext.endswith(".xml") or ext.endswith(".log"):
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
请为下面的文件内容生成 3 个标签，标签要简短、概括核心内容，并用英文逗号分隔：
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

def main():
    fp = sys.argv[1]
    text = extract_text(fp)
    if text:
        tags = tag_text_qwen(text)
        print(json.dumps({"file": fp, "tags":tags}))
    else:
        print("跳过（无法解析）：", fp)

if __name__ == "__main__":
    main()
