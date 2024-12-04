import { bleOperationQueue } from './ble.queue.js';
import { BleCore } from './ble.core.js';
import { BleFirmware } from './ble.firmware.js';
import { BleSettings } from './ble.settings.js';
import { BleDataTransfer } from './ble.data-transfer.js';
import { BleStatus } from './ble.status.js';
import { BleCommands } from './ble.commands.js';

export class BleService {
    constructor() {
        console.log('🚨 BleService constructor');
        this.core = new BleCore();
        this.firmware = new BleFirmware(this.core);
        this.settings = new BleSettings(this.core);
        this.dataTransfer = new BleDataTransfer(this.core);
        this.status = new BleStatus(this.core);
        this.commands = new BleCommands(this.core);
    }

    setConnectionStateCallback(callback) {
        this.core.setConnectionStateCallback(callback);
    }

    startStatusNotifications() {
        this.status.startStatusNotifications();
    }
    
    async connect() {
        const result = await this.core.connect();
        if (result.isConnected) {
            await this.dataTransfer.registerNotifications();
            this.status.startStatusNotifications();
        }
        return result;
    }

    async disconnect() {
        return await this.core.disconnect();
    }

    // Firmware methods
    async readFirmwareInfo() {
        return await this.firmware.readFirmwareInfo();
    }

    async measureAndSetMTU() {
        return await this.firmware.measureAndSetMTU();
    }

    // Settings methods
    async readSettings() {
        return await this.settings.readSettings();
    }

    async updateSettings(settings) {
        return await this.settings.updateSettings(settings);
    }

    // Data transfer methods
    async requestWifiNetworks() {
        return await this.dataTransfer.requestWifiNetworks();
    }

    async requestWifiDevices() {
        return await this.dataTransfer.requestWifiDevices();
    }

    async requestBleDevices() {
        return await this.dataTransfer.requestBleDevices();
    }

    // Status methods
    async readDeviceStatus() {
        return await this.status.readDeviceStatus();
    }

    // Command methods
    async saveData() {
        return await this.commands.saveData();
    }

    async saveWifiNetworks() {
        return await this.commands.saveWifiNetworks();
    }

    async saveWifiDevices() {
        return await this.commands.saveWifiDevices();
    }

    async saveBleDevices() {
        return await this.commands.saveBleDevices();
    }

    async clearData() {
        return await this.commands.clearData();
    }

    async resetDevice() {
        return await this.commands.resetDevice();
    }

    // Device info methods
    async fetchDeviceInfo() {
        try {
            return {
                mtu: await this.measureAndSetMTU(),
                firmwareInfo: await this.readFirmwareInfo(),
                deviceStatus: await this.readDeviceStatus(),
                settings: await this.readSettings()
            };
        } catch (error) {
            console.error('Error fetching device info:', error);
            throw error;
        }
    }

    // Core getters
    get isConnected() {
        return this.core.isConnected;
    }

    get deviceName() {
        return this.core.deviceName;
    }
}
