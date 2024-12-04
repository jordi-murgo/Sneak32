export class BleSettings {
    constructor(bleCore) {
        this.core = bleCore;
    }

    async readSettings() {
        return this.core.queue.enqueue(async () => {
            try {
                const value = await this.core.characteristics.settings.readValue();
                return this.parseSettings(new TextDecoder().decode(value));
            } catch (error) {
                throw new Error(`Error reading settings: ${error.message}`);
            }
        }, 'Read Settings');
    }

    async updateSettings(settings) {
        return this.core.queue.enqueue(async () => {
            try {
                const encoder = new TextEncoder();
                const settingsString = this.serializeSettings(settings);
                await this.core.characteristics.settings.writeValue(encoder.encode(settingsString));
                console.log('🚨 Settings updated:', settingsString);
                return true;
            } catch (error) {
                throw new Error(`Error updating settings: ${error.message}`);
            }
        }, 'Update Settings');
    }

    parseSettings(settingsString) {
        console.log('👓 Parsing settings:', settingsString);
        const parts = settingsString.split('|');

        const appSettings = {
            onlyManagementFrames: parts[0] === '1',
            minimalRSSI: parseInt(parts[1]),
            dwellTime: parseInt(parts[2]),
            bleScanPeriod: parseInt(parts[3]),
            ignoreRandomBLE: parts[4] === '1',
            bleScanDuration: parseInt(parts[5]),
            operationMode: parseInt(parts[6]),
            passiveScan: parts[7] === '1',
            stealthMode: parts[8] === '1',
            autosaveInterval: parseInt(parts[9]),
            authorizedMAC: parts[10] || null,
            cpuSpeed: parseInt(parts[11]) || 80,
            ledMode: parseInt(parts[12]) || 1,
            wifiTxPower: parseInt(parts[13]) || 20,
            bleTxPower: parseInt(parts[14]) || 0,
            bleMTU: parseInt(parts[15]) || 512,
            ignoreLocalWifiAddresses: parseInt(parts[16]) === 1,
            deviceName: parts.slice(17).join('|') || 'Unknown Device'
        };

        console.log('Parsed settings:', appSettings);
        return appSettings;
    }

    serializeSettings(settings) {
        return [
            settings.onlyManagementFrames ? '1' : '0',
            settings.minimalRSSI,
            settings.dwellTime,
            settings.bleScanPeriod,
            settings.ignoreRandomBLE ? '1' : '0',
            settings.bleScanDuration,
            settings.operationMode,
            settings.passiveScan ? '1' : '0',
            settings.stealthMode ? '1' : '0',
            settings.autosaveInterval,
            settings.authorizedMAC || '',
            settings.cpuSpeed,
            settings.ledMode,
            settings.wifiTxPower,
            settings.bleTxPower,
            settings.bleMTU,
            settings.ignoreLocalWifiAddresses ? '1' : '0',
            settings.deviceName
        ].join('|');
    }
}
