# MouseMove

## Why Use This Program?

MouseMove was created to solve a specific problem with remote desktop software like Moonlight: some applications don't accept simulated or emulated mouse inputs. These applications require direct USB device communication to function properly. While VirtualHere enables USB device sharing over a network, manually connecting and disconnecting devices can be cumbersome.

This program automatically manages VirtualHere USB device connections based on window focus, ensuring that:
1. Your USB device (like a mouse) only connects to the remote computer when you're actually using the remote application
2. The device automatically disconnects when you switch to local applications
3. You don't have to manually manage VirtualHere connections

This is particularly useful for:
- Gaming setups using Moonlight/Sunshine
- Remote workstations requiring precise input
- Applications that reject simulated input devices
- Setups where you frequently switch between local and remote use

## Features

- Automatically manages VirtualHere USB device connections based on window focus
- Monitors specific window titles for focus changes
- Maintains connection status through heartbeat system
- Configurable through external configuration file
- Provides warning notifications for connection issues

## Requirements

- Windows 64-bit operating system
- VirtualHere USB Client 
- VirtualHere USB Server (vhusbdwin64.exe)
- Visual C++ Redistributable for Visual Studio 2015-2022
- LAN or VLAN connectivity between the Client/Host.

# Installation Guide

## Server Installation (Remote computer with USB Device)

1. Download MouseMoveR.exe from the latest release
2. Create a config.txt file in the same directory with these settings:
   ```
   DEVICE_ID=YOUR_VIRTUALHERE_DEVICE_ID
   SERVER_PORT=8080
   POLL_INTERVAL=5
   HEARTBEAT_TIMEOUT=17
   WARNING_INTERVAL=60
   ```
3. Replace YOUR_DEVICE_ID with your VirtualHere device ID (can be found in VirtualHere Client)
4. Run MouseMoveR.exe
5. Allow through Windows Firewall if prompted

## Client Installation (Computer running Moonlight)

1. Download MouseMove.exe from the latest release
2. Create a config.txt file in the same directory with these settings:
   ```
   SERVER_IP=SERVER_COMPUTER_IP
   SERVER_PORT=8080
   HEARTBEAT_INTERVAL=5
   POLL_INTERVAL=2
   VHUSB_CHECK_INTERVAL=30
   WINDOW_TITLE=COMPUTER_NAME - Moonlight
   ```
3. Replace SERVER_COMPUTER_IP with the IP address of the computer running MouseMoveR
4. Replace the WINDOW_TITLE if necessary with your Moonlight window title
5. Run MouseMove.exe

## Troubleshooting

1. Ensure VirtualHere USB Client is running on both computers
2. Check Windows Firewall settings if connection fails
3. Verify IP addresses and ports in config files
4. Check that the device ID matches exactly what's shown in VirtualHere Client
5. Monitor the console output for any error messages

## Building from Source

1. Clone the repository
2. Open the solution in Visual Studio
3. Build both MouseMove and MouseMoveR projects
4. Output executables will be in the Debug/Release folders

## License

This project is licensed under the MIT License - see the LICENSE file for details

## Contributing

1. Fork the repository
2. Create a new branch for your feature
3. Submit a pull request

## Support

For issues and feature requests, please use the GitHub issues system.