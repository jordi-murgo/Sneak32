export class BleStatus {
    constructor(bleCore) {
        this.core = bleCore;
        this.statusInterval = null;
        this.statusCallback = null;
    }

    async readDeviceStatus() {
        return this.core.queue.enqueue(async () => {
            try {
                const value = await this.core.characteristics.deviceStatus.readValue();
                return this.parseDeviceStatus(new TextDecoder().decode(value));
            } catch (error) {
                throw new Error(`Error reading status: ${error.message}`);
            }
        }, 'Read Device Status');
    }

    startStatusNotifications() {
        this.core.characteristics.deviceStatus.addEventListener(
            'characteristicvaluechanged',
            (event) => this.handleStatusNotification(event)
        );
        this.core.characteristics.deviceStatus.startNotifications();
    }

    handleStatusNotification(event) {
        const value = event.target.value;
        const decoder = new TextDecoder();
        const status = this.parseDeviceStatus(decoder.decode(value));
        this.core.dispatchEvent('device-status-update', { status });
    }

    parseDeviceStatus(statusString) {
        console.log('📊 Parsing device status:', statusString);
        const parts = statusString.split(':');
        return { 
            wifiNetworks: parseInt(parts[0]), 
            wifiDevices: parseInt(parts[1]), 
            bleDevices: parseInt(parts[2]), 
            wifiDetectedNetworks: parseInt(parts[3]), 
            wifiDetectedDevices: parseInt(parts[4]), 
            bleDetectedDevices: parseInt(parts[5]), 
            alarm: parseInt(parts[6]) === 1, 
            freeHeap: parseInt(parts[7]), 
            uptime: parseInt(parts[8]) 
        };
    }
}
