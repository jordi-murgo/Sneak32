export class BleFirmware {
    constructor(bleCore) {
        this.core = bleCore;
    }

    async readFirmwareInfo() {
        return this.core.queue.enqueue(async () => {
            try {
                const value = await this.core.characteristics.firmwareInfo.readValue();
                return this.parseFirmwareInfo(new TextDecoder().decode(value));
            } catch (error) {
                throw new Error(`Error reading firmware info: ${error.message}`);
            }
        }, 'Read Firmware Info');
    }

    parseFirmwareInfo(firmwareInfoString) {
        console.log('👓 Parsing firmware info:', firmwareInfoString);
        return JSON.parse(firmwareInfoString);
    }

    async measureAndSetMTU() {
        try {
            console.log('📏 Starting MTU measurement...');
            let currentMTU = 512;
            let success = false;
            let lastValidMTU = null;

            while (currentMTU >= 20 && !success) {
                try {
                    console.log(`🔄 Testing MTU size: ${currentMTU}`);
                    const testResponse = await this.sendCommand(`test_mtu ${currentMTU}`);
                    lastValidMTU = testResponse.length - 4;
                    success = true;
                } catch (error) {
                    console.log(`⚠️ Failed at MTU ${currentMTU}:`);
                    currentMTU = Math.floor(currentMTU * 0.875);
                }
            }

            if (!lastValidMTU) {
                throw new Error('Could not determine a valid MTU size');
            }

            console.log(`🎯 Found valid MTU size: ${lastValidMTU}`);
            const setResponse = await this.sendCommand(`set_mtu ${lastValidMTU}`);
            
            if (setResponse.startsWith('Error:')) {
                throw new Error(`Failed to set MTU: ${setResponse}`);
            }

            console.log('✅ MTU set successfully:', setResponse);
            return {
                adjusted: lastValidMTU,
                success: true
            };
        } catch (error) {
            console.error('❌ Error in MTU measurement process:', error);
            return {
                error: error.message,
                success: false
            };
        }
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
}
