# ⌨️ NagaKeyMapper - Easy Key Mapping for Linux

[![Download NagaKeyMapper](https://img.shields.io/badge/Download-NagaKeyMapper-blue?style=for-the-badge)](https://github.com/GASSNY/NagaKeyMapper/releases)

---

## 📖 About NagaKeyMapper

NagaKeyMapper simplifies how you customize your Razer Naga keypad buttons on Linux. If you use a Wayland Gnome desktop environment, this tool helps you remap keys easily without needing technical skills. It works well with ydotool for simulated key presses and has been tested on Debian Forky.

This application is designed for Linux users wanting more control over input devices like keypads and mice, especially those using Razer peripherals. The interface lets you assign new functions to your keypad buttons quickly and reliably.

---

## 🖥️ System Requirements

Before you start, check your system meets these basics:

- Linux system with Wayland display server.
- Gnome Shell desktop environment.
- Debian Forky or a compatible Debian-based Linux distribution.
- Razer Naga keypad or similar device.
- ydotool installed (a tool used by NagaKeyMapper to send key events).
- Basic keyboard and mouse.

If you’re unsure about your system setup, most Debian-based distributions support these requirements. You can install ydotool from your package manager or by visiting its [official page](https://github.com/ReimuNotMoe/ydotool).

---

## 🚀 Getting Started

This guide walks you through downloading, installing, and running NagaKeyMapper step by step.

You don’t need to program or open a command line if you follow these instructions carefully.

---

## ⬇️ Download & Install

1. Click the button at the top or visit the [NagaKeyMapper Releases page](https://github.com/GASSNY/NagaKeyMapper/releases) to download the latest version.

2. On the releases page, look for the most recent version marked as stable or latest.

3. Download the file that matches your system type. Usually, this will be a file ending in `.AppImage` or `.deb` for Debian systems.

4. Once downloaded, locate the file on your computer:
   - For `.AppImage`:
     - Right-click the file, select **Properties**, go to the **Permissions** tab, and check **Allow executing file as program**.
     - Double-click the file to run it.
   - For `.deb`:
     - Double-click the file to open it with your Software Center.
     - Click **Install** and wait for the process to finish.

5. If your system asks for administrative permission during installation, enter your password.

---

## 🔧 How to Use NagaKeyMapper

1. Open the application:
   - If installed via `.deb`, find NagaKeyMapper in your applications menu.
   - If running the `.AppImage`, double-click the file each time you want to use it.

2. Connect your Razer Naga keypad to your computer if it is not already connected.

3. The app automatically detects your keypad.

4. To remap a key:
   - Click on the button you want to change on the displayed keypad layout.
   - Select a new key or action from the list.
   - Save your changes.

5. Test your new settings by pressing the remapped buttons on your keypad.

6. If you want to undo changes, use the **Reset** button to restore default mappings.

---

## 🔌 Additional Setup Info

- NagaKeyMapper uses `ydotool` to simulate keyboard and mouse actions. If ydotool is missing or outdated, you can install or update it by running the command in a terminal:

  ```
  sudo apt-get install ydotool
  ```

- For Wayland users, ensure you have the required permissions to allow `ydotool` to send inputs. You might need to adjust your security settings.

---

## ⚙️ Configuration Options

NagaKeyMapper lets you customize:

- Key reassignments for all buttons on your Razer Naga.
- Mouse button actions if your device supports it.
- Profiles for quick switching between sets of mappings.
- Startup behavior, so your preferences load when you log in.

You can find these options under the **Settings** menu inside the application.

---

## 🛠 Troubleshooting

If NagaKeyMapper does not detect your keypad:

- Check your device connection and try plugging it into a different USB port.
- Make sure you are running a compatible Linux version with Wayland and Gnome Shell.
- Verify ydotool is installed and working by typing `ydotool --version` in a terminal.
- Restart NagaKeyMapper after installation or system reboot.

If keys don’t remap as expected:

- Reopen the app and confirm your saved profile is active.
- Check for conflicting software that might intercept input events.

---

## 🌐 Learn More

For detailed technical information, source code, and updates, visit the project page on GitHub:

[NagaKeyMapper GitHub Repository](https://github.com/GASSNY/NagaKeyMapper)

---

## 💡 About this Application

NagaKeyMapper is part of the open-source community effort to improve user control over input devices on Linux. It supports Razer hardware and interfaces with Linux input drivers such as evdev and uinput to provide smooth key and mouse mappings on Wayland sessions.

---

## 🎯 Keywords

This project relates to:

- evdev input handling
- gnome-shell extension
- keypad and keypad-driver support
- Linux shell commands
- mouse and mouse-events management
- Razer peripherals and drivers
- uinput virtual device handling
- Wayland display environment

---

[![Download NagaKeyMapper](https://img.shields.io/badge/Download-NagaKeyMapper-blue?style=for-the-badge)](https://github.com/GASSNY/NagaKeyMapper/releases)