import { bleOperationQueue } from './ble.queue.js';

export class BleCore {
    static SNEAK32_SERVICE_UUID = "81af4cd7-e091-490a-99ee-caa99032ef4e";
    static FIRMWARE_INFO_UUID = 0xFFE3;
    static SETTINGS_UUID = 0xFFE2;
    static DATA_TRANSFER_UUID = 0xFFE0;
    static COMMANDS_UUID = 0xFFE4;
    static STATUS_UUID = 0xFFE1;

    constructor() {
        console.log('🚨 BleCore constructor');
        this.device = null;
        this.server = null;
        this.characteristics = {
            firmwareInfo: null,
            settings: null,
            dataTransfer: null,
            commands: null,
            deviceStatus: null
        };
        this.isIntentionalDisconnect = false;
        this.onConnectionStateChange = null;
        this.connectionPromise = null;
        this.queue = bleOperationQueue;
    }

    setConnectionStateCallback(callback) {
        this.onConnectionStateChange = callback;
    }

    async connect() {
        console.log('🔌 Connecting to device...');
        if (this.connectionPromise) {
            return this.connectionPromise;
        }

        this.connectionPromise = (async () => {
            try {
                if (this.isConnected) {
                    console.log('🔌 Already connected');
                    return {
                        deviceName: this.device.name,
                        isConnected: true
                    };
                }

                this.device = await navigator.bluetooth.requestDevice({
                    filters: [{ services: [BleCore.SNEAK32_SERVICE_UUID] }],
                    optionalServices: [
                        BleCore.FIRMWARE_INFO_UUID,
                        BleCore.SETTINGS_UUID,
                        BleCore.DATA_TRANSFER_UUID,
                        BleCore.COMMANDS_UUID,
                        BleCore.STATUS_UUID
                    ]
                });

                this.device.addEventListener('gattserverdisconnected', (event) => this.handleDisconnection(event));
                this.server = await this.device.gatt.connect();

                await this.initializeCharacteristics();

                return {
                    deviceName: this.device.name,
                    isConnected: true
                };

            } catch (error) {
                if (this.onConnectionStateChange) {
                    this.onConnectionStateChange(false);
                }
                throw error;
            } finally {
                this.connectionPromise = null;
            }
        })();

        return this.connectionPromise;
    }

    async initializeCharacteristics() {
        const service = await this.server.getPrimaryService(BleCore.SNEAK32_SERVICE_UUID);
        
        this.characteristics = {
            firmwareInfo: await service.getCharacteristic(BleCore.FIRMWARE_INFO_UUID),
            settings: await service.getCharacteristic(BleCore.SETTINGS_UUID),
            dataTransfer: await service.getCharacteristic(BleCore.DATA_TRANSFER_UUID),
            commands: await service.getCharacteristic(BleCore.COMMANDS_UUID),
            deviceStatus: await service.getCharacteristic(BleCore.STATUS_UUID)
        };

        console.log('🚨 Characteristics initialized:', this.characteristics);
    }

    async disconnect() {
        try {
            if (this.device?.gatt.connected) {
                this.isIntentionalDisconnect = true;
                await this.device.gatt.disconnect();
                this.cleanup();
                return true;
            }
            return false;
        } catch (error) {
            throw new Error(`Disconnection failed: ${error.message}`);
        } finally {
            this.isIntentionalDisconnect = false;
        }
    }

    cleanup() {
        this.queue.clear();
        this.device = null;
        this.server = null;
        this.characteristics = {
            firmwareInfo: null,
            settings: null,
            dataTransfer: null,
            commands: null,
            status: null
        };
    }

    handleDisconnection(event) {
        console.log('🚨 Disconnection detected:', event);
        this.queue.clear();
        
        const wasConnected = this.isConnected;
        const wasIntentional = this.isIntentionalDisconnect;
        
        this.cleanup();
        
        this.dispatchEvent('disconnected', { unexpected: !wasIntentional });
        if (this.onConnectionStateChange) {
            this.onConnectionStateChange(false);
        }
    }

    dispatchEvent(eventName, detail) {
        const event = new CustomEvent(eventName, {
            detail,
            bubbles: true,
            composed: true
        });
        document.dispatchEvent(event);
    }

    get isConnected() {
        return this.device?.gatt.connected || false;
    }

    get deviceName() {
        return this.device?.name || null;
    }
}
