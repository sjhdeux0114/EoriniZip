"""Report changed vendored upstreams; never silently replace patched codecs."""
import json
import os
import pathlib
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPOS = {"unalz": "alkegi/unalz", "unegg": "alkegi/unegg",
         "easycrab": "kjy00302/easycrab", "zpaq": "zpaq/zpaq"}
changed = False
lines = ["## Native codec upstream review"]
for name, repo in REPOS.items():
    pinned = (ROOT / "third_party" / name / "UPSTREAM_COMMIT").read_text().strip()
    headers = {"User-Agent": "EoriniZip-maintenance", "Accept": "application/vnd.github+json"}
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = "Bearer " + token
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repo}/commits?per_page=1", headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        latest = json.load(response)[0]["sha"]
    status = "CURRENT" if latest == pinned else "REVIEW REQUIRED"
    changed |= latest != pinned
    line = f"{name}: {status} (pinned {pinned}, latest {latest})"
    print(line)
    lines.append("- " + line)
if os.environ.get("GITHUB_STEP_SUMMARY"):
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
        summary.write("\n".join(lines) + "\n")
raise SystemExit(1 if changed else 0)
