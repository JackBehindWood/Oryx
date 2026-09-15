"""
Oryx Build System

A simple Python-based build helper that manages dependencies (Premake5)
and orchestrates the build process.

Usage:
    python build.py configure   # Check and install dependencies
    python build.py build       # Generate build files and compile
    python build.py clean       # Clean build artifacts
    python build.py test        # Run tests
    python build.py all         # Configure, build, and test
"""

import os
import sys
import subprocess
import platform
import json
import urllib.request
import zipfile
import tarfile
from pathlib import Path


class BuildConfig:
    """Build configuration and dependency management."""

    def __init__(self):
        self.root_dir = Path(__file__).parent.absolute()
        self.build_dir = self.root_dir / "build"
        self.bin_dir = self.root_dir / "bin"
        self.premake_dir = self.root_dir / "premake"
        self.config_file = self.root_dir / "build_config.json"
        self.os_type = platform.system()
        self.arch = platform.machine()
        self.config = self._load_config()

    def _load_config(self):
        """Load build configuration from file."""
        if self.config_file.exists():
            with open(self.config_file, 'r') as f:
                return json.load(f)
        return self._default_config()

    def _default_config(self):
        """Return default build configuration."""
        return {
            "project_name": "oryx",
            "dependencies": {
                "premake5": {
                    "required": True,
                    "version": "5.0.0-beta2",
                    "urls": {
                        "Linux": "https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz",
                        "Darwin": "https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-macosx.tar.gz",
                        "Windows": "https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip"
                    },
                    "license": "https://raw.githubusercontent.com/premake/premake-core/master/LICENSE.txt"
                }
            },
            "build_generator": "gmake2",
            "test_executable": "oryx_tests"
        }

    def save_config(self):
        """Save current configuration to file."""
        with open(self.config_file, 'w') as f:
            json.dump(self.config, f, indent=2)


class DependencyManager:
    """Manage project dependencies."""

    def __init__(self, config):
        self.config = config
        self.os_type = config.os_type
        self.root_dir = config.root_dir
        self.premake_dir = config.premake_dir

    def get_premake_executable(self):
        """Get the path to the premake5 executable."""
        if self.os_type == "Windows":
            return self.premake_dir / "premake5.exe"
        else:
            return self.premake_dir / "premake5"

    def check_local_premake(self):
        """Check if premake5 is available locally."""
        premake_exe = self.get_premake_executable()
        if premake_exe.exists():
            return True
        return False

    def check_system_premake(self):
        """Check if premake5 exists in system PATH."""
        try:
            subprocess.run(
                ["premake5", "--version"],
                capture_output=True,
                timeout=5,
                check=False
            )
            return True
        except FileNotFoundError:
            return False

    def download_premake5(self):
        """Download and extract Premake5."""
        print(f"📥 Downloading Premake5 v{self.config.config['dependencies']['premake5']['version']}...\n")

        urls = self.config.config["dependencies"]["premake5"]["urls"]
        if self.os_type not in urls:
            print(f"✗ Unsupported OS: {self.os_type}")
            return False

        url = urls[self.os_type]
        license_url = self.config.config["dependencies"]["premake5"]["license"]

        self.premake_dir.mkdir(parents=True, exist_ok=True)

        # Download Premake5
        try:
            print(f"⏳ Downloading from {url}...")
            filename = url.split("/")[-1]
            filepath = self.premake_dir / filename

            urllib.request.urlretrieve(url, filepath)
            print(f"✓ Downloaded {filename}\n")

            # Extract
            print(f"📦 Extracting {filename}...")
            if filename.endswith(".tar.gz"):
                with tarfile.open(filepath, "r:gz") as tar:
                    tar.extractall(path=self.premake_dir)
            elif filename.endswith(".zip"):
                with zipfile.ZipFile(filepath, "r") as zip_ref:
                    zip_ref.extractall(path=self.premake_dir)
            else:
                print(f"✗ Unknown archive format: {filename}")
                return False

            print(f"✓ Extracted successfully\n")

            # Make executable on Unix
            if self.os_type in ["Linux", "Darwin"]:
                premake_exe = self.get_premake_executable()
                premake_exe.chmod(0o755)
                print(f"✓ Made executable: {premake_exe}\n")

            # Clean up archive
            filepath.unlink()

            # Download license
            print(f"📄 Downloading Premake5 license...")
            try:
                license_path = self.premake_dir / "LICENSE.txt"
                urllib.request.urlretrieve(license_url, license_path)
                print(f"✓ Saved license to {license_path}\n")
            except Exception as e:
                print(f"⚠️  Could not download license: {e}\n")

            return True

        except Exception as e:
            print(f"✗ Failed to download/extract Premake5: {e}")
            return False

    def ensure_dependencies(self):
        """Ensure all required dependencies are available."""
        print("🔍 Checking dependencies...\n")

        premake_info = self.config.config["dependencies"]["premake5"]
        if not premake_info.get("required", True):
            return True

        # Check local first
        if self.check_local_premake():
            print(f"✓ premake5: found locally at {self.get_premake_executable()}\n")
            return True

        # Check system
        if self.check_system_premake():
            print(f"✓ premake5: found in system PATH\n")
            return True

        # Download and install
        print(f"✗ premake5: NOT FOUND\n")
        if self.download_premake5():
            print(f"✓ premake5: installed locally\n")
            return True
        else:
            print("\n⚠️  Failed to install Premake5 automatically.")
            print("Please install manually: https://premake.github.io/download.html")
            return False


class Builder:
    """Orchestrate the build process."""

    def __init__(self, config):
        self.config = config
        self.root_dir = config.root_dir
        self.build_dir = config.build_dir
        self.bin_dir = config.bin_dir
        self.premake_dir = config.premake_dir

    def get_premake_command(self):
        """Get the premake5 command to run."""
        local_premake = self.premake_dir / ("premake5.exe" if self.config.os_type == "Windows" else "premake5")
        if local_premake.exists():
            return str(local_premake)
        return "premake5"

    def configure(self):
        """Generate build files with Premake5."""
        print("⚙️  Configuring build...\n")

        self.build_dir.mkdir(exist_ok=True)

        generator = self.config.config.get("build_generator", "gmake2")
        premake_cmd = self.get_premake_command()
        cmd = [premake_cmd, generator]

        try:
            result = subprocess.run(
                cmd,
                cwd=str(self.root_dir),
                capture_output=True,
                text=True,
                check=True
            )
            print("✓ Build files generated successfully\n")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Failed to configure build:")
            print(e.stderr)
            return False
        except FileNotFoundError:
            print("✗ Premake5 not found. Ensure it was installed correctly.")
            return False

    def build(self):
        """Compile the project."""
        print("🔨 Building project...\n")

        if not self.build_dir.exists():
            print("✗ Build directory not found. Run 'python build.py configure' first.")
            return False

        generator = self.config.config.get("build_generator", "gmake2")

        if generator == "gmake2":
            cmd = ["make", "-C", str(self.build_dir)]
        else:
            print(f"✗ Unsupported generator: {generator}")
            return False

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            print(result.stdout)
            print("✓ Build successful\n")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Build failed:")
            print(e.stdout)
            print(e.stderr)
            return False
        except FileNotFoundError:
            print(f"✗ Build tool not found. Ensure {generator} is installed.")
            return False

    def clean(self):
        """Clean build artifacts."""
        print("🧹 Cleaning build artifacts...\n")

        if self.build_dir.exists():
            import shutil
            shutil.rmtree(self.build_dir)
            print(f"✓ Removed {self.build_dir}")

        if self.bin_dir.exists():
            import shutil
            shutil.rmtree(self.bin_dir)
            print(f"✓ Removed {self.bin_dir}")

        print()

    def test(self):
        """Run tests."""
        print("🧪 Running tests...\n")

        test_exec = self.config.config.get("test_executable", "oryx_tests")
        test_path = self.bin_dir / test_exec

        if not test_path.exists():
            print(f"✗ Test executable not found: {test_path}")
            print("  Run 'python build.py build' first.")
            return False

        try:
            result = subprocess.run(
                [str(test_path)],
                capture_output=True,
                text=True,
                check=True
            )
            print(result.stdout)
            print("✓ Tests passed\n")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Tests failed:")
            print(e.stdout)
            print(e.stderr)
            return False
        except FileNotFoundError:
            print(f"✗ Could not run {test_path}")
            return False


def main():
    """Main build entry point."""
    if len(sys.argv) < 2:
        print(__doc__)
        print("Commands:")
        print("  configure   Check and install dependencies, generate build files")
        print("  build       Compile the project")
        print("  clean       Remove build artifacts")
        print("  test        Run tests")
        print("  all         Configure, build, and test")
        sys.exit(0)

    config = BuildConfig()
    dep_manager = DependencyManager(config)
    builder = Builder(config)

    command = sys.argv[1].lower()

    if command == "configure":
        if dep_manager.ensure_dependencies():
            if not builder.configure():
                sys.exit(1)
        else:
            sys.exit(1)

    elif command == "build":
        if not builder.configure():
            sys.exit(1)
        if not builder.build():
            sys.exit(1)

    elif command == "clean":
        builder.clean()

    elif command == "test":
        if not builder.test():
            sys.exit(1)

    elif command == "all":
        if dep_manager.ensure_dependencies():
            if builder.configure() and builder.build() and builder.test():
                print("✓ All tasks completed successfully!")
            else:
                sys.exit(1)
        else:
            sys.exit(1)

    else:
        print(f"Unknown command: {command}")
        print("Run 'python build.py' for usage information.")
        sys.exit(1)


if __name__ == "__main__":
    main()
