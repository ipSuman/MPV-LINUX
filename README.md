# MPV-Linux

MPV-Linux is a lightweight native Linux desktop media player built around **libmpv**, **Qt 6**, and **CMake**. It is designed as an easy-to-use, MX Player–style player for Ubuntu/Linux.

## ✨ Features

### 🎬 Playback
- ▶️ Play / Pause
- ⏮️ Previous / Next media
- ⏪ −10 sec and ⏩ +10 sec seek buttons
- 👆 Double-click left/right third for fixed −10/+10 second seeking
- 🖱️ Mouse-wheel seeking
- ⌨️ Configurable keyboard seeking
- ⏭️ Frame-by-frame stepping
- 🔁 Playlist autoplay

### 📂 Playlist
- 📄 Add files
- 📁 Add an entire folder
- 🖱️ Drag & drop files
- 🗑️ Clear playlist
- 🎯 Current selection is preserved when adding media

### 🔊 Audio
- 🔊 Volume control
- ⬆️ Customizable Volume Up shortcut
- ⬇️ Customizable Volume Down shortcut
- 🔇 Customizable Mute shortcut

### 🖼️ Video
- 🔍 Zoom
- 🖐️ Pan zoomed video
- 🎨 Brightness adjustment
- 🌈 Contrast adjustment
- 🎚️ Saturation adjustment
- 🖥️ Hardware / Software decoding toggle
- ℹ️ Video Information dialog

### 🔄 Loop & Editing
- 🔵 A/B Loop
- ✂️ Cut selected A/B section
- 🎞️ FFmpeg stream-copy cutting without re-encoding

### 🖥️ Display
- ⛶ Fullscreen mode
- 🖱️ Mouse pointer automatically hides after 5 seconds over the video
- 👀 Controls reappear when the mouse moves
- ⌨️ F11 / Enter / Numpad Enter fullscreen
- 🚪 Escape exits fullscreen

### ⚙️ Customization
- 🎛️ Configurable controls
- ⌨️ Custom keyboard shortcuts
- ⏱️ Configurable seek duration: 5 sec, 10 sec, 30 sec, or 1–120 min
- 🖱️ Configurable mouse-wheel behavior
- 🖐️ Configurable pan button
- 👆 Configurable double-click behavior
- 💾 Persistent settings

### 🛠️ Diagnostics
- 📝 Save Log diagnostic report
- 📊 Playback/media information
- 🐞 Shortcut and configuration information in logs

## 📦 Installation on Ubuntu/Debian

### Recommended: install the `.deb` package

Download **`mpv-linux_0.1.0_amd64.deb`** from the GitHub Releases page, then open a terminal in the download directory and run:

```bash
sudo apt install ./mpv-linux_0.1.0_amd64.deb
```

`apt` will install the required runtime dependencies automatically.

After installation, launch **MPV-Linux** from your application menu, or run:

```bash
rex-player
```

### Uninstall

```bash
sudo apt remove mpv-linux
```

### Requirements

The packaged application currently targets **64-bit x86 Ubuntu/Debian systems (amd64)**. The `.deb` package installs the application launcher and registers it in the desktop application menu.

The package depends on Qt 6, libmpv and FFmpeg. FFmpeg is used for the A/B stream-copy cutting feature.

## 📦 Portable installation

The release also includes `mpv-linux.tar.gz` for users who prefer a portable package without installing a `.deb`.

Extract it and run:

```bash
tar -xzf mpv-linux.tar.gz
cd mpv-linux
./rex-player
```

The launcher uses XCB/XWayland for compatibility with the legacy libmpv window-embedding path and sets the required numeric locale for mpv.

## 🛠️ Build from source

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config qt6-base-dev libmpv-dev ffmpeg

cmake -S . -B build
cmake --build build -j$(nproc)
./build/rex-player
```

Open a file directly:

```bash
./build/rex-player /path/to/video.mkv
```

## ✂️ A-B cutting

Set A and B with the configured A/B shortcuts, then click **Cut AB**. Cutting uses FFmpeg stream copy (`-c copy`) and does not re-encode the media. Because stream copy is keyframe-limited, the beginning of a cut may be slightly before the requested A point.

## 🖥️ Hardware decoding

The **HW/SW** button shows whether hardware decoding is active. Clicking it toggles mpv between `hwdec=auto` and `hwdec=no`.

## 📄 License

See the repository for the current project licensing information.
