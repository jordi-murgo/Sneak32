export class BleCommands {
    constructor(bleCore) {
        this.core = bleCore;
    }

    async sendCommand(command) {
        return this.core.queue.enqueue(async () => {
            const encoder = new TextEncoder();
            await this.core.characteristics.commands.writeValue(encoder.encode(command));
            console.log('🚨 Command sent:', command);
            
            await new Promise(resolve => setTimeout(resolve, 10));
            const value = await this.core.characteristics.commands.readValue();
            const response = new TextDecoder().decode(value);
            
            console.log('🚨 Command response:', response);
            if(response.startsWith('Error:')) {
                throw new Error(response);
            }
            return response;
        }, `Send Command: ${command}`);
    }

    async saveData() {
        return await this.sendCommand('save_data');
    }

    async saveWifiNetworks() {
        return await this.sendCommand('save_wifi_networks');
    }

    async saveWifiDevices() {
        return await this.sendCommand('save_wifi_devices');
    }

    async saveBleDevices() {
        return await this.sendCommand('save_ble_devices');
    }

    async clearData() {
        return await this.sendCommand('clear_data');
    }

    async resetDevice() {
        await this.sendCommand('restart');
    }
}
