# LCD Display Support for Sneak32

Sneak32 now supports LCD displays via LVGL (Light and Versatile Graphics Library), providing a rich graphical interface for monitoring WiFi networks, BLE devices, and system status without requiring a web browser.

## Hardware Support

The display support is designed for ESP32-S3 boards with attached LCD displays, particularly:

- **Waveshare ESP32-S3-Touch-LCD-5B** (5" 800x480 IPS display)
- **Waveshare ESP32-S3-Touch-LCD-7** (7" 1024x600 IPS display)
- Other ESP32-S3 boards with compatible TFT displays

## Features

### Multiple Display Modes
- **Overview**: System status and device counts
- **WiFi Networks**: List of detected WiFi networks with RSSI and channel info
- **WiFi Devices**: List of detected WiFi devices (stations)
- **BLE Devices**: List of detected Bluetooth Low Energy devices
- **Status**: System messages and detailed information

### Real-time Updates
- Automatic refresh of device lists during scanning
- Scanning status indicators
- Live RSSI and device count updates

### BLE Command Interface
New commands available via BLE:
- `display_mode <0-4>`: Switch between display modes
- `display_status`: Show current display configuration

## Configuration

### Build Configuration

1. **Enable Display Support**: Use the `esp32-s3-display` environment
   ```bash
   pio run -e esp32-s3-display
   ```

2. **Hardware-specific settings**: Modify `src/User_Setup.h` for your display:
   ```cpp
   // Pin configuration for your board
   #define TFT_MISO 12
   #define TFT_MOSI 11
   #define TFT_SCLK 14
   #define TFT_CS   10
   #define TFT_DC   13
   #define TFT_RST  21
   #define TFT_BL   38  // Backlight control
   ```

### Runtime Configuration

Display settings are stored in preferences and can be configured via BLE:

- **Enable/Disable**: `appPrefs.enable_display`
- **Brightness**: `appPrefs.display_brightness` (0-255)
- **Rotation**: `appPrefs.display_rotation` (0-3)
- **Timeout**: `appPrefs.display_timeout` (seconds)

## Usage Examples

### Waveshare ESP32-S3-Touch-LCD-5B Setup

1. Flash firmware with display support:
   ```bash
   pio run -e esp32-s3-display -t upload
   ```

2. Connect via BLE and configure:
   ```
   display_mode 1  # Show WiFi networks
   display_mode 3  # Show BLE devices
   ```

3. The display will automatically update with detected devices during scanning.

### Custom Display Configuration

For other display hardware, update `src/User_Setup.h`:

```cpp
// Example for different display
#define ST7796_DRIVER
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
```

## Technical Details

### Architecture
- **DisplayManager**: Core class handling LVGL and display operations
- **LVGL Integration**: Full graphics library with efficient rendering
- **TFT_eSPI Driver**: Hardware abstraction for various display controllers
- **Conditional Compilation**: Zero overhead when display is disabled

### Memory Usage
- Display buffers: ~64KB (configurable)
- LVGL memory pool: 64KB (configurable)
- Minimal impact on core scanning functionality

### Performance
- Non-blocking display updates
- Efficient partial screen refreshes
- Configurable update intervals
- Maintains real-time scanning performance

## Troubleshooting

### Display Not Working
1. Check pin configuration in `User_Setup.h`
2. Verify `ENABLE_DISPLAY=1` in build flags
3. Check serial output for initialization messages

### Compilation Issues
1. Ensure LVGL and TFT_eSPI libraries are installed
2. Check that `lv_conf.h` is properly included
3. Verify platform and board configuration

### Performance Issues
1. Reduce display update frequency
2. Lower display resolution if needed
3. Adjust LVGL memory settings

## Future Enhancements

- Touch input support for interactive navigation
- Configurable display themes
- Additional display controllers
- Real-time graphs and charts
- Custom widget layouts

## Contributing

When adding display features:
1. Maintain compatibility with non-display builds
2. Use conditional compilation (`#ifdef ENABLE_DISPLAY`)
3. Follow existing architecture patterns
4. Test on both display and non-display configurations