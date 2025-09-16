#include "DisplayUI.h"

#ifdef ENABLE_DISPLAY

#include <algorithm>
#include <cstdio>
#include "AppPreferences.h"

namespace
{
constexpr std::size_t kMaxTableRows = 8;

lv_color_t badgeColorForMode(uint8_t mode)
{
    switch (mode)
    {
    case OPERATION_MODE_SCAN:
        return lv_palette_main(LV_PALETTE_BLUE);
    case OPERATION_MODE_DETECTION:
        return lv_palette_main(LV_PALETTE_DEEP_ORANGE);
    default:
        return lv_palette_main(LV_PALETTE_GREY);
    }
}

} // namespace

void DisplayUI::create(const DisplaySummaryInfo &summary)
{
    if (created_)
    {
        return;
    }

    createLayout(summary);
    created_ = true;
}

void DisplayUI::update(const DisplaySummaryInfo &summary,
                       const std::vector<WifiNetworkView> &networks,
                       const std::vector<WifiDeviceView> &stations,
                       const std::vector<BleDeviceView> &bleDevices)
{
    if (!created_)
    {
        create(summary);
    }

    updateSummary(summary);
    updateWifiTable(networks);
    updateStationTable(stations);
    updateBleTable(bleDevices);
}

void DisplayUI::createLayout(const DisplaySummaryInfo &summary)
{
    screen_ = lv_scr_act();
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0e141b), 0);
    lv_obj_set_style_bg_grad_color(screen_, lv_color_hex(0x151d26), 0);
    lv_obj_set_style_bg_grad_dir(screen_, LV_GRAD_DIR_VER, 0);

    // Header with device name and mode badge
    lv_obj_t *header = lv_obj_create(screen_);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_style_pad_all(header, 20, 0);
    lv_obj_set_style_pad_row(header, 12, 0);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    deviceNameLabel_ = lv_label_create(header);
    lv_obj_set_style_text_font(deviceNameLabel_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(deviceNameLabel_, lv_color_hex(0xffffff), 0);

    modeBadge_ = lv_label_create(header);
    lv_label_set_long_mode(modeBadge_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_color(modeBadge_, badgeColorForMode(summary.operationMode), 0);
    lv_obj_set_style_bg_opa(modeBadge_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(modeBadge_, 14, 0);
    lv_obj_set_style_pad_hor(modeBadge_, 14, 0);
    lv_obj_set_style_pad_ver(modeBadge_, 8, 0);
    lv_obj_set_style_text_color(modeBadge_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(modeBadge_, &lv_font_montserrat_18, 0);

    // Stats container
    lv_obj_t *stats = lv_obj_create(screen_);
    lv_obj_remove_style_all(stats);
    lv_obj_set_width(stats, LV_PCT(100));
    lv_obj_set_style_bg_color(stats, lv_color_hex(0x141c24), 0);
    lv_obj_set_style_bg_opa(stats, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(stats, 18, 0);
    lv_obj_set_style_pad_all(stats, 24, 0);
    lv_obj_set_style_pad_column(stats, 32, 0);
    lv_obj_set_style_pad_row(stats, 18, 0);
    lv_obj_set_layout(stats, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    wifiCountLabel_ = createStatCard(stats, "WiFi Networks", lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    stationCountLabel_ = createStatCard(stats, "WiFi Devices", lv_palette_main(LV_PALETTE_AMBER));
    bleCountLabel_ = createStatCard(stats, "BLE Devices", lv_palette_main(LV_PALETTE_DEEP_PURPLE));

    lastUpdateLabel_ = createStatCard(stats, "Last Refresh", lv_palette_main(LV_PALETTE_GREY));
    channelLabel_ = createStatCard(stats, "Channel", lv_palette_main(LV_PALETTE_CYAN));
    detectionLabel_ = createStatCard(stats, "Detection", lv_palette_main(LV_PALETTE_RED));

    // Tables container
    lv_obj_t *tables = lv_obj_create(screen_);
    lv_obj_remove_style_all(tables);
    lv_obj_set_width(tables, LV_PCT(100));
    lv_obj_set_style_pad_all(tables, 0, 0);
    lv_obj_set_style_pad_gap(tables, 20, 0);
    lv_obj_set_layout(tables, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tables, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(tables, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    wifiTable_ = createTableSection(tables, "WiFi Networks", 5);
    lv_obj_set_width(lv_obj_get_parent(wifiTable_), LV_PCT(100));

    stationTable_ = createTableSection(tables, "Stations", 4);
    lv_obj_set_width(lv_obj_get_parent(stationTable_), LV_PCT(48));

    bleTable_ = createTableSection(tables, "BLE Devices", 4);
    lv_obj_set_width(lv_obj_get_parent(bleTable_), LV_PCT(48));

    // Configure headers and column widths
    configureTableHeader(wifiTable_, {"SSID", "RSSI", "Ch", "Type", "Seen"});
    configureTableHeader(stationTable_, {"MAC", "RSSI", "Ch", "Seen"});
    configureTableHeader(bleTable_, {"Device", "Public", "RSSI", "Seen"});

    lv_table_set_column_width(wifiTable_, 0, 360);
    lv_table_set_column_width(wifiTable_, 1, 90);
    lv_table_set_column_width(wifiTable_, 2, 70);
    lv_table_set_column_width(wifiTable_, 3, 140);
    lv_table_set_column_width(wifiTable_, 4, 90);

    lv_table_set_column_width(stationTable_, 0, 240);
    lv_table_set_column_width(stationTable_, 1, 90);
    lv_table_set_column_width(stationTable_, 2, 70);
    lv_table_set_column_width(stationTable_, 3, 90);

    lv_table_set_column_width(bleTable_, 0, 260);
    lv_table_set_column_width(bleTable_, 1, 90);
    lv_table_set_column_width(bleTable_, 2, 90);
    lv_table_set_column_width(bleTable_, 3, 90);

    updateSummary(summary);
}

void DisplayUI::updateSummary(const DisplaySummaryInfo &summary)
{
    std::string name = "Sneak32";
    if (!summary.deviceName.empty())
    {
        name += " • " + summary.deviceName;
    }
    lv_label_set_text(deviceNameLabel_, name.c_str());

    std::string modeText;
    switch (summary.operationMode)
    {
    case OPERATION_MODE_SCAN:
        modeText = "Scan";
        break;
    case OPERATION_MODE_DETECTION:
        modeText = "Detection";
        break;
    default:
        modeText = "Idle";
        break;
    }
    if (summary.passiveScan)
    {
        modeText += " • Passive";
    }
    if (summary.stealthMode)
    {
        modeText += " • Stealth";
    }
    if (summary.deviceConnected)
    {
        modeText += " • BLE Link";
    }
    lv_label_set_text(modeBadge_, modeText.c_str());
    lv_obj_set_style_bg_color(modeBadge_, badgeColorForMode(summary.operationMode), 0);

    lv_label_set_text(wifiCountLabel_, formatCount(summary.wifiNetworkCount).c_str());
    lv_label_set_text(stationCountLabel_, formatCount(summary.wifiDeviceCount).c_str());
    lv_label_set_text(bleCountLabel_, formatCount(summary.bleDeviceCount).c_str());
    lv_label_set_text(lastUpdateLabel_, formatDuration(summary.lastRefreshMillis).c_str());
    lv_label_set_text(channelLabel_, formatChannel(summary.currentChannel).c_str());

    std::string detection = "Standby";
    if (summary.operationMode == OPERATION_MODE_DETECTION)
    {
        detection = summary.detectionTarget.empty() ? "Auto" : summary.detectionTarget;
        if (summary.detectionAlarm)
        {
            detection += " • ALERT";
            lv_obj_set_style_text_color(detectionLabel_, lv_palette_main(LV_PALETTE_RED), 0);
        }
        else
        {
            lv_obj_set_style_text_color(detectionLabel_, lv_color_hex(0xffffff), 0);
        }
    }
    else
    {
        lv_obj_set_style_text_color(detectionLabel_, lv_color_hex(0xffffff), 0);
    }
    lv_label_set_text(detectionLabel_, detection.c_str());
}

void DisplayUI::updateWifiTable(const std::vector<WifiNetworkView> &networks)
{
    lv_table_set_row_cnt(wifiTable_, kMaxTableRows + 1);
    for (std::size_t i = 0; i < kMaxTableRows; ++i)
    {
        if (i < networks.size())
        {
            const auto &item = networks[i];
            lv_table_set_cell_value(wifiTable_, i + 1, 0, item.ssid.c_str());
            lv_table_set_cell_value(wifiTable_, i + 1, 1, formatRssi(item.rssi).c_str());
            lv_table_set_cell_value(wifiTable_, i + 1, 2, formatChannel(item.channel).c_str());
            lv_table_set_cell_value(wifiTable_, i + 1, 3, item.type.c_str());
            lv_table_set_cell_value(wifiTable_, i + 1, 4, formatTimes(item.timesSeen).c_str());
        }
        else
        {
            for (int col = 0; col < 5; ++col)
            {
                lv_table_set_cell_value(wifiTable_, i + 1, col, "");
            }
        }
    }
}

void DisplayUI::updateStationTable(const std::vector<WifiDeviceView> &stations)
{
    lv_table_set_row_cnt(stationTable_, kMaxTableRows + 1);
    for (std::size_t i = 0; i < kMaxTableRows; ++i)
    {
        if (i < stations.size())
        {
            const auto &item = stations[i];
            lv_table_set_cell_value(stationTable_, i + 1, 0, item.mac.c_str());
            lv_table_set_cell_value(stationTable_, i + 1, 1, formatRssi(item.rssi).c_str());
            lv_table_set_cell_value(stationTable_, i + 1, 2, formatChannel(item.channel).c_str());
            lv_table_set_cell_value(stationTable_, i + 1, 3, formatTimes(item.timesSeen).c_str());
        }
        else
        {
            for (int col = 0; col < 4; ++col)
            {
                lv_table_set_cell_value(stationTable_, i + 1, col, "");
            }
        }
    }
}

void DisplayUI::updateBleTable(const std::vector<BleDeviceView> &devices)
{
    lv_table_set_row_cnt(bleTable_, kMaxTableRows + 1);
    for (std::size_t i = 0; i < kMaxTableRows; ++i)
    {
        if (i < devices.size())
        {
            const auto &item = devices[i];
            lv_table_set_cell_value(bleTable_, i + 1, 0, formatBleName(item).c_str());
            lv_table_set_cell_value(bleTable_, i + 1, 1, item.isPublic ? "Yes" : "Random");
            lv_table_set_cell_value(bleTable_, i + 1, 2, formatRssi(item.rssi).c_str());
            lv_table_set_cell_value(bleTable_, i + 1, 3, formatTimes(item.timesSeen).c_str());
        }
        else
        {
            for (int col = 0; col < 4; ++col)
            {
                lv_table_set_cell_value(bleTable_, i + 1, col, "");
            }
        }
    }
}

lv_obj_t *DisplayUI::createStatCard(lv_obj_t *parent, const char *title, lv_color_t accentColor)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1b252f), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_hor(card, 20, 0);
    lv_obj_set_style_pad_ver(card, 16, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *caption = lv_label_create(card);
    lv_obj_set_style_text_color(caption, accentColor, 0);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_16, 0);
    lv_label_set_text(caption, title);

    lv_obj_t *value = lv_label_create(card);
    lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);
    lv_label_set_text(value, "--");
    return value;
}

lv_obj_t *DisplayUI::createTableSection(lv_obj_t *parent, const char *title, uint8_t columnCount)
{
    lv_obj_t *section = lv_obj_create(parent);
    lv_obj_remove_style_all(section);
    lv_obj_set_style_bg_color(section, lv_color_hex(0x141c24), 0);
    lv_obj_set_style_bg_opa(section, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(section, 18, 0);
    lv_obj_set_style_pad_all(section, 24, 0);
    lv_obj_set_style_pad_row(section, 16, 0);
    lv_obj_set_layout(section, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *titleLabel = lv_label_create(section);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xffffff), 0);
    lv_label_set_text(titleLabel, title);

    lv_obj_t *table = lv_table_create(section);
    lv_obj_set_width(table, LV_PCT(100));
    lv_obj_set_style_bg_opa(table, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(table, 1, 0);
    lv_obj_set_style_border_color(table, lv_color_hex(0x1f2a33), 0);
    lv_obj_set_style_pad_all(table, 0, 0);
    lv_table_set_column_cnt(table, columnCount);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_16, 0);
    return table;
}

void DisplayUI::configureTableHeader(lv_obj_t *table, const std::vector<std::string> &headers)
{
    lv_table_set_row_cnt(table, 1);
    for (std::size_t i = 0; i < headers.size(); ++i)
    {
        lv_table_set_cell_value(table, 0, static_cast<int>(i), headers[i].c_str());
        lv_table_set_cell_align(table, 0, static_cast<int>(i), LV_TEXT_ALIGN_LEFT);
    }
    lv_obj_set_style_text_color(table, lv_color_hex(0xffffff), 0);
}

std::string DisplayUI::formatCount(size_t value)
{
    char buffer[16];
    if (value >= 1'000'000)
    {
        std::snprintf(buffer, sizeof(buffer), "%.1fM", value / 1'000'000.0);
    }
    else if (value >= 1'000)
    {
        std::snprintf(buffer, sizeof(buffer), "%.1fk", value / 1'000.0);
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(value));
    }
    return buffer;
}

std::string DisplayUI::formatRssi(int rssi)
{
    char buffer[16];
    if (rssi == 0)
    {
        return "--";
    }
    std::snprintf(buffer, sizeof(buffer), "%d dBm", rssi);
    return buffer;
}

std::string DisplayUI::formatTimes(uint32_t times)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(times));
    return buffer;
}

std::string DisplayUI::formatChannel(int channel)
{
    if (channel <= 0)
    {
        return "--";
    }
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%d", channel);
    return buffer;
}

std::string DisplayUI::formatDuration(uint32_t millis)
{
    uint32_t seconds = millis / 1000U;
    uint32_t hours = seconds / 3600U;
    uint32_t minutes = (seconds % 3600U) / 60U;
    uint32_t secs = seconds % 60U;

    char buffer[32];
    if (hours > 0)
    {
        std::snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, secs);
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "%02u:%02u", minutes, secs);
    }
    return buffer;
}

std::string DisplayUI::formatBleName(const BleDeviceView &device)
{
    if (!device.name.empty())
    {
        return device.name;
    }
    if (!device.address.empty())
    {
        return device.address;
    }
    return "Unknown";
}

#endif // ENABLE_DISPLAY
