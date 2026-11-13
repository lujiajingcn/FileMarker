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

# ---- 服务端配置（由 C++ 端通过环境变量传入）----
# FM_AI_PROVIDER = ollama（本机 Ollama，默认）| api（任意 OpenAI 兼容接口）
PROVIDER = (os.environ.get("FM_AI_PROVIDER") or "ollama").strip().lower()
if PROVIDER not in ("ollama", "api"):
    PROVIDER = "ollama"

# 模型名可用环境变量覆盖（换更小/更大的模型不用改脚本）
TEXT_MODEL = os.environ.get("FM_AI_TEXT_MODEL", "qwen2.5:7b")
IMAGE_MODEL = os.environ.get("FM_AI_IMAGE_MODEL", "qwen2.5vl:7b")

API_BASE = (os.environ.get("FM_AI_API_BASE") or "").strip().rstrip("/")
API_KEY = os.environ.get("FM_AI_API_KEY") or ""
API_PROXY = (os.environ.get("FM_AI_API_PROXY") or "").strip()

try:
    TIMEOUT = max(10, min(600, int(os.environ.get("FM_AI_TIMEOUT", "120"))))
except ValueError:
    TIMEOUT = 120

OLLAMA_URL = "http://localhost:11434"


class DependencyError(RuntimeError):
    pass


class CloudApiError(RuntimeError):
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


def split_model_tags(full_resp):
    """模型可能用中文逗号/顿号/分号分隔，统一拆开再清洗。"""
    tags = [tag.strip() for tag in re.split(r"[,，、;；\n]", full_resp) if tag.strip()]
    return clean_tags(tags)


# ============================================================
# 本地 Ollama
# ============================================================
def start_ollama_llm(commond):
    """在后台启动 Ollama 服务。"""
    print("尝试启动 Ollama 服务...", file=sys.stderr)
    try:
        process = subprocess.Popen(
            commond,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        print(f"Ollama 服务已尝试启动，进程 ID: {process.pid}", file=sys.stderr)
        time.sleep(3)
        if process.poll() is None:
            print("Ollama 服务似乎已成功启动。", file=sys.stderr)
            return process
        print(f"Ollama 启动失败，退出代码: {process.returncode}", file=sys.stderr)
        return None
    except FileNotFoundError:
        print("错误: 找不到 Ollama 可执行文件。", file=sys.stderr)
        return None
    except Exception as e:
        print(f"启动 Ollama 时发生意外错误: {e}", file=sys.stderr)
        return None


def is_ollama_running(url=OLLAMA_URL):
    try:
        response = requests.get(f"{url}/api/tags", timeout=5)
        return response.status_code == 200
    except (ConnectionError, Timeout):
        return False
    except Exception:
        return False


def is_model_available(model, url=OLLAMA_URL):
    """检查指定模型是否已通过 ollama 安装，避免缺失模型时静默跳过。"""
    try:
        r = requests.get(f"{url}/api/tags", timeout=5)
        if r.status_code != 200:
            return False
        models = [m.get("name", "") for m in r.json().get("models", [])]
        return any(model == m or model.startswith(m + ":") or m.startswith(model + ":")
                   for m in models)
    except Exception:
        return False


def _ollama_generate(payload):
    """调用 Ollama /api/generate（流式多行 JSON），把 response 拼起来。"""
    r = requests.post(f"{OLLAMA_URL}/api/generate", json=payload, stream=True, timeout=TIMEOUT)
    full_resp = ""
    for line in r.iter_lines():
        if not line:
            continue
        try:
            obj = json.loads(line.decode("utf-8"))
        except Exception:
            continue
        if "error" in obj:
            raise RuntimeError(obj["error"])
        if "response" in obj:
            full_resp += obj["response"]
    return full_resp


def build_text_prompt(text):
    return (
        "请为下面的文件内容生成3个中文标签，标签要简短、概括核心内容，并用,分隔：\n"
        + (text or "")[:4000]
    )


IMAGE_PROMPT = "请为下面的图片生成3个中文标签，标签要简短、概括核心内容，并用,分隔："


def tag_text_ollama(text):
    payload = {
        "model": TEXT_MODEL,
        "prompt": build_text_prompt(text),
        "options": {"temperature": 0.3, "num_predict": 64},
    }
    return split_model_tags(_ollama_generate(payload))


def tag_image_ollama(file_path):
    try:
        with open(file_path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("utf-8")
    except Exception as e:
        raise RuntimeError(f"读取图片失败：{e}")
    payload = {
        "model": IMAGE_MODEL,
        "prompt": IMAGE_PROMPT,
        "images": [b64],
        "options": {"temperature": 0.3, "num_predict": 64},
    }
    return split_model_tags(_ollama_generate(payload))


# ============================================================
# 云端 API（OpenAI 兼容：/chat/completions）
# ============================================================
def cloud_headers():
    headers = {"Content-Type": "application/json"}
    if API_KEY:
        # 注意：Key 只放在请求头里，绝不能写进日志/输出
        headers["Authorization"] = "Bearer " + API_KEY
    return headers


def _cloud_host():
    try:
        from urllib.parse import urlparse
        return urlparse(API_BASE).hostname or API_BASE
    except Exception:
        return API_BASE


def _cloud_proxies():
    # 设置里填了代理就用它；没填则沿用环境变量 HTTPS_PROXY / HTTP_PROXY
    if API_PROXY:
        return {"http": API_PROXY, "https": API_PROXY}
    return None


def _post_with_retry(url, payload):
    """连接被重置这类瞬时错误重试一次；仍失败则给出可读原因。"""
    last = None
    for attempt in range(2):
        try:
            return requests.post(url, json=payload, headers=cloud_headers(),
                                 timeout=TIMEOUT, proxies=_cloud_proxies())
        except Timeout:
            last = "timeout"
        except ConnectionError as e:
            last = str(e)
        if attempt == 0:
            time.sleep(1.0)
    if last == "timeout":
        raise CloudApiError(f"请求超时（{TIMEOUT} 秒）：{_cloud_host()}")
    raise CloudApiError(
        "连接不上服务器 %s（连接被重置/中断）。\n"
        "常见原因：网络或防火墙拦截、需要代理、或 DNS 被劫持。\n"
        "可尝试：换一个网络（如手机热点）；或在【文件 → AI 打标设置…】里填写代理后重试。" % _cloud_host())


def _cloud_request(payload):
    if not API_BASE:
        raise CloudApiError("云端模式未配置 Base URL（请在【AI 打标设置】里填写）")

    url = API_BASE + "/chat/completions"
    r = _post_with_retry(url, payload)

    if r.status_code == 401 or r.status_code == 403:
        raise CloudApiError(f"API Key 无效或没有权限（HTTP {r.status_code}）")
    if r.status_code == 402:
        raise CloudApiError("账户余额不足（HTTP 402）")
    if r.status_code == 429:
        raise CloudApiError("被服务端限流（HTTP 429），请稍后重试")
    if r.status_code >= 400:
        detail = ""
        try:
            data = r.json()
            detail = (data.get("error") or {}).get("message") or data.get("message") or ""
        except Exception:
            detail = r.text[:200]
        raise CloudApiError(f"HTTP {r.status_code}：{detail or '请求失败'}")

    try:
        data = r.json()
        return data["choices"][0]["message"]["content"]
    except Exception:
        raise CloudApiError("服务端返回格式无法识别（不是 OpenAI 兼容格式）")


def tag_text_cloud(text):
    payload = {
        "model": TEXT_MODEL,
        "temperature": 0.2,
        "max_tokens": 64,
        "messages": [{"role": "user", "content": build_text_prompt(text)}],
    }
    return split_model_tags(_cloud_request(payload))


def tag_image_cloud(file_path):
    ext = file_path.lower()
    mime = "image/png" if ext.endswith(".png") else "image/jpeg"
    try:
        with open(file_path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("utf-8")
    except Exception as e:
        raise RuntimeError(f"读取图片失败：{e}")

    payload = {
        "model": IMAGE_MODEL,
        "temperature": 0.2,
        "max_tokens": 64,
        "messages": [{
            "role": "user",
            "content": [
                {"type": "text", "text": IMAGE_PROMPT},
                {"type": "image_url", "image_url": {"url": f"data:{mime};base64,{b64}"}},
            ],
        }],
    }
    return split_model_tags(_cloud_request(payload))


# ============================================================
# 文本提取
# ============================================================
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


def print_result(fp, tags=None, error=None):
    data = {"file": fp, "tags": tags or []}
    if error:
        data["error"] = error
    print(json.dumps(data))


def main():
    ollama_proc = None
    try:
        if len(sys.argv) < 2:
            print(json.dumps({"error": "缺少文件路径参数"}))
            return

        fp = sys.argv[1]
        kind = file_kind(fp)
        if kind == "unsupported":
            print_result(fp, error="不支持的文件类型：%s" % (os.path.splitext(fp)[1] or "无扩展名"))
            return

        # ---------------- 云端 API ----------------
        if PROVIDER == "api":
            model = IMAGE_MODEL if kind == "image" else TEXT_MODEL
            if not model:
                print_result(fp, error="云端模式没有配置%s模型（请在【AI 打标设置】里填写）"
                                       % ("视觉" if kind == "image" else "文本"))
                return
            try:
                if kind == "image":
                    tags = tag_image_cloud(fp)
                else:
                    text = extract_text(fp)
                    if not text or not text.strip():
                        print_result(fp, error="没有从文件里提取到可分析的文字")
                        return
                    tags = tag_text_cloud(text)
            except DependencyError as e:
                print_result(fp, error="缺少依赖：%s" % e)
                return
            except (CloudApiError, RuntimeError) as e:
                print_result(fp, error="处理失败：%s" % e)
                return
            except Exception as e:
                print_result(fp, error="处理失败：%s" % e)
                return
            print_result(fp, tags=tags)
            return

        # ---------------- 本地 Ollama ----------------
        if not is_ollama_running():
            ollama_proc = start_ollama_llm(["ollama", "serve"])

        model = IMAGE_MODEL if kind == "image" else TEXT_MODEL
        if not is_model_available(model):
            print_result(fp, error="模型 %s 未安装，请先执行 `ollama pull %s`" % (model, model))
            return

        try:
            if kind == "image":
                tags = tag_image_ollama(fp)
            else:
                text = extract_text(fp)
                if not text or not text.strip():
                    print_result(fp, error="没有从文件里提取到可分析的文字")
                    return
                tags = tag_text_ollama(text)
        except DependencyError as e:
            print_result(fp, error="缺少依赖：%s" % e)
            return
        except RuntimeError as e:
            print_result(fp, error="处理失败：%s" % e)
            return
        except Exception as e:
            print_result(fp, error="处理失败：%s" % e)
            return

        print_result(fp, tags=tags)

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
