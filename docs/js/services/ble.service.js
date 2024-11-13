export class BleService {
    static SNEAK32_SERVICE_UUID = "81af4cd7-e091-490a-99ee-caa99032ef4e";
    static FIRMWARE_INFO_UUID = 0xFFE3;
    static SETTINGS_UUID = 0xFFE2;
    static DATA_TRANSFER_UUID = 0xFFE0;
    static COMMANDS_UUID = 0xFFE4;
    static STATUS_UUID = 0xFFE1;

    constructor() {
        console.log('🚨 BleService constructor');
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
        this.statusInterval = null;
        this.dataTransferCallback = null;
        this.statusCallback = null;
        this.onConnectionStateChange = null;
        this.connectionPromise = null;
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
                    filters: [{ services: [BleService.SNEAK32_SERVICE_UUID] }],
                    optionalServices: [
                        BleService.FIRMWARE_INFO_UUID,
                        BleService.SETTINGS_UUID,
                        BleService.DATA_TRANSFER_UUID,
                        BleService.COMMANDS_UUID,
                        BleService.STATUS_UUID
                    ]
                });

                this.device.addEventListener('gattserverdisconnected', () => this.handleDisconnection());
                this.server = await this.device.gatt.connect();

                await this.initializeCharacteristics();
                await this.registerNotifications();

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
        const service = await this.server.getPrimaryService(BleService.SNEAK32_SERVICE_UUID);
        
        this.characteristics = {
            firmwareInfo: await service.getCharacteristic(BleService.FIRMWARE_INFO_UUID),
            settings: await service.getCharacteristic(BleService.SETTINGS_UUID),
            dataTransfer: await service.getCharacteristic(BleService.DATA_TRANSFER_UUID),
            commands: await service.getCharacteristic(BleService.COMMANDS_UUID),
            deviceStatus: await service.getCharacteristic(BleService.STATUS_UUID)
        };

        console.log('🚨 Characteristics initialized:', this.characteristics);
    }

    async registerNotifications() {
        this.characteristics.dataTransfer.addEventListener(
            'characteristicvaluechanged',
            (event) => this.handleDataTransferNotification(event)
        );
        await this.characteristics.dataTransfer.startNotifications();
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
        clearInterval(this.statusInterval);
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

    handleDisconnection() {
        clearInterval(this.statusInterval);
        
        const wasConnected = this.isConnected;
        const wasIntentional = this.isIntentionalDisconnect;
        
        this.cleanup();
        
        if (wasConnected) {
            if (!wasIntentional) {
                this.dispatchEvent('disconnected', { unexpected: true });
            } else {
                this.dispatchEvent('disconnected', { unexpected: false });
            }
            if (this.onConnectionStateChange) {
                this.onConnectionStateChange(false);
            }
        }
    }

    async readFirmwareInfo() {
        try {
            const value = await this.characteristics.firmwareInfo.readValue();
            return this.parseFirmwareInfo(new TextDecoder().decode(value));
        } catch (error) {
            throw new Error(`Error reading firmware info: ${error.message}`);
        }
    }

    async readDeviceStatus() {
        try {
            const value = await this.characteristics.deviceStatus.readValue();
            return this.parseDeviceStatus(new TextDecoder().decode(value));
        } catch (error) {
            throw new Error(`Error reading status: ${error.message}`);
        }
    }

    async readSettings() {
        try {
            const value = await this.characteristics.settings.readValue();
            return this.parseSettings(new TextDecoder().decode(value));
        } catch (error) {
            throw new Error(`Error reading settings: ${error.message}`);
        }
    }

    async updateSettings(settings) {
        try {
            const encoder = new TextEncoder();
            const settingsString = this.serializeSettings(settings);
            await this.characteristics.settings.writeValue(encoder.encode(settingsString));
            console.log('🚨 Settings updated:', settingsString);
            return true;
        } catch (error) {
            throw new Error(`Error updating settings: ${error.message}`);
        }
    }

    async sendCommand(command) {
        const encoder = new TextEncoder();
        await this.characteristics.commands.writeValue(encoder.encode(command));
        console.log('🚨 Command sent:', command);
        // Wait for the response to be available
        await new Promise(resolve => setTimeout(resolve, 10));
        var response = new TextDecoder().decode( await this.characteristics.commands.readValue() );
        console.log('🚨 Command response:', response);
        if(response.startsWith('Error:')) {
            throw new Error(response);
        }
        return response;
    }

    async getData(initialPacket = '0000') {
        try {
            const encoder = new TextEncoder();
            await this.characteristics.dataTransfer.writeValue(encoder.encode(initialPacket));
            return true;
        } catch (error) {
            throw new Error(`Error getting data: ${error.message}`);
        }
    }

    // Event handling methods
    handleDataTransferNotification(event) {
        const value = event.target.value;
        const decoder = new TextDecoder();
        const data = decoder.decode(value);
        if(this.dataTransferCallback) {
            this.dataTransferCallback(data);
        } else {
            console.log('🚨 No data transfer callback registered:', data);
        }
    }

    handleStatusNotification(event) {
        const value = event.target.value;
        const decoder = new TextDecoder();
        const status = this.parseDeviceStatus(decoder.decode(value));
        this.dispatchEvent('device-status-update', { status });
    }

    // String format: "1|-100|10000|30|1|15|1|0|0|60||80|1|34|11|508|Sneak32-C3 (EF30)"
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
            authorizedMAC: parts[10] || null, // Manejar caso de que no haya MAC autorizada
            cpuSpeed: parseInt(parts[11]) || 80,
            ledMode: parseInt(parts[12]) || 1,
            wifiTxPower: parseInt(parts[13]) || 20,
            bleTxPower: parseInt(parts[14]) || 0,
            bleMTU: parseInt(parts[15]) || 256,
            deviceName: parts.slice(16).join('|') || 'Unknown Device', // Manejar caso de que no haya nombre
        };

        console.log('Parsed settings:', appSettings);
        return appSettings;
    }

    // JSON format.
    parseFirmwareInfo(firmwareInfoString) {
        console.log('👓 Parsing firmware info:', firmwareInfoString);
        return JSON.parse(firmwareInfoString);
    }

    // String format: "50:100:9:0:0:0:0:95612:3596"
    parseDeviceStatus(statusString) {
        console.log('👓 Parsing device status:', statusString);
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
            settings.deviceName
        ].join('|');
    }

    dispatchEvent(eventName, detail) {
        const event = new CustomEvent(eventName, {
            detail,
            bubbles: true,
            composed: true
        });
        document.dispatchEvent(event);
    }

    // Getters
    get isConnected() {
        return this.device?.gatt.connected || false;
    }

    get deviceName() {
        return this.device?.name || null;
    }

    async measureAndSetMTU() {
        try {
            console.log('📏 Starting MTU measurement...');
            let currentMTU = 600; // Comenzamos con el máximo
            let success = false;
            let lastValidMTU = null;

            while (currentMTU >= 20 && !success) {
                try {
                    console.log(`🔄 Testing MTU size: ${currentMTU}`);
                    const testResponse = await this.sendCommand(`test_mtu ${currentMTU}`);

                    // Si llegamos aquí, la prueba fue exitosa
                    lastValidMTU = testResponse.length - 4;
                    success = true;
                    
                } catch (error) {
                    console.log(`⚠️ Failed at MTU ${currentMTU}:`);
                    currentMTU = Math.floor(currentMTU * 0.875); // Reducimos a 7/8
                }
            }

            if (!lastValidMTU) {
                throw new Error('Could not determine a valid MTU size');
            }

            console.log(`🎯 Found valid MTU size: ${lastValidMTU}`);
            
            // Intentamos establecer el MTU encontrado
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

    async requestWifiNetworks() {
        try {
            console.log('📥 Requesting WiFi networks...');
            // Iniciar la transferencia solicitando SSIDs
            await this.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode('ssid_list')
            );

            // Esperar y procesar los datos
            const data = await this.receivePackets();

            console.log('📥 WiFi networks:', data);

            return JSON.parse(data);
        } catch (error) {
            throw new Error(`Error requesting WiFi networks: ${error.message}`);
        }
    }

    async requestWifiDevices() {
        try {
            console.log('📥 Requesting WiFi devices...');
            // Iniciar la transferencia solicitando clientes WiFi
            await this.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode('client_list')
            );

            // Esperar y procesar los datos
            const data = await this.receivePackets();
            return JSON.parse(data);
        } catch (error) {
            throw new Error(`Error requesting WiFi devices: ${error.message}`);
        }
    }

    async requestBleDevices() {
        try {
            console.log('📥 Requesting BLE devices...');
            // Iniciar la transferencia solicitando dispositivos BLE
            await this.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode('ble_list')
            );

            // Esperar y procesar los datos
            const data = await this.receivePackets();
            return JSON.parse(data);
        } catch (error) {
            throw new Error(`Error requesting BLE devices: ${error.message}`);
        }
    }

    async receivePackets() {
        return new Promise((resolve, reject) => {
            let completeData = '';
            let currentPacket = 1;
            let timeout;

            const cleanup = () => {
                clearTimeout(timeout);
                this.dataTransferCallback = null;
            };

            // Establecer un timeout global para la transferencia
            timeout = setTimeout(() => {
                cleanup();
                reject(new Error('Data transfer timeout'));
            }, 30000); // 30 segundos de timeout

            this.dataTransferCallback = (data) => {
                console.log('📥 Data transfer callback:', data);
                
                if (data.startsWith('START:')) {
                    // Extraer el número total de paquetes del mensaje START
                    const totalPackets = parseInt(data.split(':')[1]);
                    console.log(`📦 Starting transfer with ${totalPackets} packets`);
                    
                    // Solicitar el primer paquete
                    this.requestNextPacket(currentPacket);
                } else if (data === 'END') {
                    cleanup();
                    try {
                        resolve(completeData);
                    } catch (error) {
                        reject(new Error(`Error parsing JSON: ${error.message}`));
                    }
                } else {
                    // Acumular datos
                    completeData += data.slice(4);
                    currentPacket++;
                    
                    // Solicitar el siguiente paquete
                    this.requestNextPacket(currentPacket);
                }
            };
        });
    }

    async requestNextPacket(packetNumber) {
        try {
            // Convertir el número de paquete a una cadena hexadecimal de 4 caracteres
            const packetHex = packetNumber.toString(16).padStart(4, '0');
            await this.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode(packetHex)
            );
        } catch (error) {
            console.error('Error requesting next packet:', error);
            throw error;
        }
    }

    startStatusNotifications() {
        this.characteristics.deviceStatus.addEventListener(
            'characteristicvaluechanged',
            (event) => this.handleStatusNotification(event)
        );
        this.characteristics.deviceStatus.startNotifications();
    }

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
}
