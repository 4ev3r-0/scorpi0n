# 1. Get the Xtensa toolchain (one-time, ~500MB)
mkdir -p ~/toolchains && cd ~/toolchains
curl -sL https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20241119/xtensa-esp-elf-14.2.0_20241119-x86_64-linux-gnu.tar.xz | tar xJ
export PATH="$PWD/xtensa-esp-elf-14.2.0_20241119/bin:$PATH"

# Add to ~/.bashrc:
# export PATH="$HOME/toolchains/xtensa-esp-elf-14.2.0_20241119/bin:$PATH"

# 2. Get esptool (just the flasher)
pip install esptool   
