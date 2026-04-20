"""PlatformIO pre-build script: enable DEBUG_I2C on non-main branches."""
import subprocess

Import("env")

try:
    branch = (
        subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            stderr=subprocess.DEVNULL,
        )
        .decode()
        .strip()
    )
except Exception:
    branch = "main"

if branch != "main":
    env.Append(CPPDEFINES=["DEBUG_I2C"])
    print(f"[debug_flags] branch={branch} → DEBUG_I2C enabled")
else:
    print(f"[debug_flags] branch=main → DEBUG_I2C disabled")
