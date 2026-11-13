import os
import re
import sys
import base64
import json
import subprocess
import time

# pypdf / python-docx / Pillow / pytesseract 改成"用到才导入"：
# 以前在文件开头就 import 全部，结果只要缺 Pillow，连纯 .txt 文件都跑不了。
import requests
from requests.exceptions import ConnectionError, Timeout

TEXT_EXTS = (".txt", ".csv", ".json", ".xml", ".log", ".md")
PDF_EXTS = (".pdf",)
DOCX_EXTS = (".docx",)
IMAGE_EXTS = (".png", ".jpg", ".jpeg")


class DependencyError(RuntimeError):
    pass


def file_kind(file_path):
    ext = file_path.lower()
    if ext.endswith(TEXT_EXTS):
        return "text"
    if ext.endswith(PDF_EXTS):
        return "pdf"
    if ext.endswith(DOCX_EXTS):
        return "docx"
    if ext.endswith(IMAGE_EXTS):
        return "image"
    return "unsupported"


def clean_tags(tags):
    result = []
    for raw in tags:
        tag = (raw or "").strip().strip("。.,;")
        tag = re.sub(r"^标签[:：\s]*", "", tag).strip()
        if not tag or len(tag) > 12:
            continue
        if tag not in result:
            result.append(tag)
    return result[:8]

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
        print(f"错误: 找不到 Ollama 可执行文件。请确保 '{commond}' 在 PATH 中或提供完整路径。")
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

def is_model_available(model, url="http://localhost:11434"):
    """检查指定模型是否已通过 ollama 安装，避免缺失模型时静默跳过（P3）。"""
    try:
        r = requests.get(f"{url}/api/tags", timeout=5)
        if r.status_code != 200:
            return False
        models = [m.get("name", "") for m in r.json().get("models", [])]
        return any(model == m or model.startswith(m + ":") or m.startswith(model + ":")
                   for m in models)
    except Exception:
        return False


def extract_text(file_path):
    ext = file_path.lower()

    # 纯文本：不需要第三方库
    if ext.endswith(TEXT_EXTS):
        with open(file_path, "r", encoding="utf8", errors="ignore") as f:
            return f.read()

    if ext.endswith(PDF_EXTS):
        try:
            from pypdf import PdfReader
        except ImportError as e:
            raise DependencyError(f"pypdf 未安装（pip install pypdf）：{e}")
        reader = PdfReader(file_path)
        return "\n".join([page.extract_text() or "" for page in reader.pages])

    if ext.endswith(DOCX_EXTS):
        try:
            from docx import Document
        except ImportError as e:
            raise DependencyError(f"python-docx 未安装（pip install python-docx）：{e}")
        doc = Document(file_path)
        return "\n".join([p.text for p in doc.paragraphs])

    # 图片交给视觉模型；其它类型由 main() 判为不支持
    return None


def tag_text_qwen(text: str):
    prompt = f"""
请为下面的文件内容生成3个中文标签，标签要简短、概括核心内容，并用,分隔：
{text[:4000]}
"""

    payload = {
        "model": TEXT_MODEL,
        "prompt": prompt,
        # 只要几个标签，限制生成长度能明显加快速度；低温让输出更稳定
        "options": {"temperature": 0.3, "num_predict": 64},
    }

    # Ollama 生成是多行 JSON，每行一个对象，因此 stream=True
    r = requests.post("http://localhost:11434/api/generate", json=payload, stream=True, timeout=120)

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

    # 清理格式，转换为列表。模型可能用中文逗号/顿号/英文分号/中文分号分隔，统一拆分
    tags = [tag.strip() for tag in re.split(r"[,，、;；]", full_resp) if tag.strip()]
    return clean_tags(tags)

def tag_image_qwen(file_path: str):
    ext = file_path.lower()

    # 非图片文件直接返回，避免对不支持的类型做无意义的请求
    if not ext.endswith((".png", ".jpg", ".jpeg")):
        return ""

    # 将图片读取为 base64 作为视觉模型的输入
    try:
        with open(file_path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("utf-8")
    except Exception as e:
        print(f"读取图片失败: {e}")
        return ""

    prompt = "请为下面的图片生成3个中文标签，标签要简短、概括核心内容，并用,分隔："

    payload = {
        "model": IMAGE_MODEL,
        "prompt": prompt,
        "images": [b64],
        "options": {"temperature": 0.3, "num_predict": 64},
    }

    # Ollama 生成是多行 JSON，每行一个对象，因此 stream=True
    r = requests.post("http://localhost:11434/api/generate", json=payload, stream=True, timeout=120)

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

    # 清理格式，转换为列表。模型可能用中文逗号/顿号/英文分号/中文分号分隔，统一拆分
    tags = [tag.strip() for tag in re.split(r"[,，、;；]", full_resp) if tag.strip()]
    return clean_tags(tags)

TEXT_MODEL = "qwen2.5:7b"
IMAGE_MODEL = "qwen2.5vl:7b"


def main():
    # 若 Ollama 未运行则尝试拉起；记录是否由本脚本启动，结束时只终止自己启动的实例，避免留下孤儿进程（P3）。
    ollama_proc = None
    if not is_ollama_running():
        # 启动 Ollama 服务（注意：应使用 `ollama serve` 而非交互式的 `ollama run`，
        # 且命令必须作为列表传入，在 Windows 上以字符串传入会被当作单个可执行文件名而失败）。
        ollama_proc = start_ollama_llm(["ollama", "serve"])

    try:
        if len(sys.argv) < 2:
            print(json.dumps({"error": "缺少文件路径参数"}))
            return

        fp = sys.argv[1]
        kind = file_kind(fp)
        if kind == "unsupported":
            print(json.dumps({"file": fp, "tags": [],
                              "error": "不支持的文件类型：%s" % (os.path.splitext(fp)[1] or "无扩展名")}))
            return

        model = IMAGE_MODEL if kind == "image" else TEXT_MODEL
        if not is_model_available(model):
            print(json.dumps({"file": fp, "tags": [],
                              "error": "模型 %s 未安装，请先执行 `ollama pull %s`" % (model, model)}))
            return

        try:
            if kind == "image":
                tags = tag_image_qwen(fp)
            else:
                text = extract_text(fp)
                if not text or not text.strip():
                    print(json.dumps({"file": fp, "tags": [],
                                      "error": "没有从文件里提取到可分析的文字"}))
                    return
                tags = tag_text_qwen(text)
        except DependencyError as e:
            print(json.dumps({"file": fp, "tags": [], "error": "缺少依赖：%s" % e}))
            return
        except Exception as e:
            print(json.dumps({"file": fp, "tags": [], "error": "处理失败：%s" % e}))
            return

        print(json.dumps({"file": fp, "tags": tags}))

    finally:
        if ollama_proc is not None:
            try:
                ollama_proc.terminate()
                ollama_proc.wait(timeout=5)
            except Exception:
                try:
                    ollama_proc.kill()
                except Exception:
                    pass


if __name__ == "__main__":
    main()
