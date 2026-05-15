from openrouter import OpenRouter
import os
import argparse
from pathlib import Path
parser = argparse.ArgumentParser(description="Test OpenRouter API")

def load_dotenv(env_path: Path) -> None:
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip().strip("\"'"))



load_dotenv(Path(__file__).with_name(".env"))

api_token = os.environ.get("HACK_CLUB_AI_KEY") or os.environ.get("OPENROUTER_API_TOKEN")


parser.add_argument(
    "--model",
    type=str,
    default="~anthropic/claude-sonnet-latest",
    help="The model to use for generation.",
)

parser.add_argument(
    "--text",
    type=str,
    help="The text to generate speech for. Mutually exclusive with --text-file.",
)

parser.add_argument(
    "--text-file",
    type=str,
    help="Path to a text file containing the text to generate speech for. Mutually exclusive with --text.",
)

parser.add_argument(
    "--sysprompt",
    type=str,
    help="System prompt to guide the model's behavior. Mutually exclusive with --sysprompt-file.",
)

parser.add_argument(
    "--sysprompt-file",
    type=str,
    help="Path to a text file containing the system prompt. Mutually exclusive with --sysprompt.",
    default="sysprompt.txt"
)

args = parser.parse_args()

client = OpenRouter(
    api_key=api_token,
    server_url="https://ai.hackclub.com/proxy/v1",
)

response = client.chat.send(
    model=args.model,
    messages=[
        {"role": "system", "content": args.sysprompt or open(
            args.sysprompt_file).read()},
        {"role": "user", "content": args.text or open(args.text_file).read()}
    ],
)

print(response.choices[0].message.content)
