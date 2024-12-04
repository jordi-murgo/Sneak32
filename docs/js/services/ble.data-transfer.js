export class BleDataTransfer {
    // Constants for binary record sizes
    static MAC_ADDR_SIZE = 6;
    static SSID_SIZE = 32;
    static TYPE_SIZE = 16;
    static NAME_SIZE = 32;
    static TIMESTAMP_SIZE = 8;
    static COUNTER_SIZE = 4;
    static RSSI_SIZE = 1;
    static CHANNEL_SIZE = 1;
    static IS_PUBLIC_SIZE = 1;

    static WIFI_NETWORK_RECORD_SIZE = this.MAC_ADDR_SIZE + this.SSID_SIZE + this.RSSI_SIZE + 
                                    this.CHANNEL_SIZE + this.TYPE_SIZE + this.TIMESTAMP_SIZE + 
                                    this.COUNTER_SIZE;
    static WIFI_DEVICE_RECORD_SIZE = this.MAC_ADDR_SIZE * 2 + this.RSSI_SIZE + this.CHANNEL_SIZE + 
                                    this.TIMESTAMP_SIZE + this.COUNTER_SIZE;
    static BLE_DEVICE_RECORD_SIZE = this.MAC_ADDR_SIZE + this.NAME_SIZE + this.RSSI_SIZE + 
                                   this.TIMESTAMP_SIZE + this.IS_PUBLIC_SIZE + this.COUNTER_SIZE;

    constructor(bleCore) {
        this.core = bleCore;
        this.dataTransferCallback = null;
        this.transferTimestamp = null;
    }

    // Helper functions for network byte order
    static readMacAddress(dataView, offset) {
        const macBytes = new Uint8Array(dataView.buffer, offset, this.MAC_ADDR_SIZE);
        return Array.from(macBytes)
            .map(b => b.toString(16).padStart(2, '0'))
            .join(':')
            .toUpperCase();
    }

    static readFixedString(dataView, offset, maxLength) {
        const bytes = new Uint8Array(dataView.buffer, offset, maxLength);
        let str = '';
        for (let i = 0; i < maxLength && bytes[i] !== 0; i++) {
            str += String.fromCharCode(bytes[i]);
        }
        return str.trim();
    }

    static readInt8(dataView, offset) {
        return dataView.getInt8(offset);
    }

    static readUint8(dataView, offset) {
        return dataView.getUint8(offset);
    }

    static readUint32(dataView, offset) {
        return dataView.getUint32(offset, false); // false = big-endian (network order)
    }

    static readUint64(dataView, offset) {
        // Read as big-endian (network order)
        const high = dataView.getUint32(offset, false);
        const low = dataView.getUint32(offset + 4, false);
        return Number((BigInt(high) << 32n) | BigInt(low));
    }

    async registerNotifications() {
        this.core.characteristics.dataTransfer.addEventListener(
            'characteristicvaluechanged',
            (event) => this.handleDataTransferNotification(event)
        );
        await this.core.characteristics.dataTransfer.startNotifications();
    }

    async receivePackets() {
        return new Promise((resolve, reject) => {
            let completeData = new Uint8Array();
            let currentPacket = 1;
            let timeout;

            const cleanup = () => {
                if(timeout) {
                    clearTimeout(timeout);
                }
                this.dataTransferCallback = null;
            };

            const updateTimeout = () => {
                if (timeout) {
                    clearTimeout(timeout);
                }
                timeout = setTimeout(() => {
                    cleanup();
                    reject(new Error('Data transfer timeout'));
                }, 5000); 
            };

            const concatenateArrays = (array1, array2) => {
                const result = new Uint8Array(array1.length + array2.length);
                result.set(array1, 0);
                result.set(array2, array1.length);
                return result;
            };

            this.dataTransferCallback = (receivedData) => {
                const data = new TextDecoder().decode(receivedData);
                console.log('📥 Data transfer callback:', data.length <= 10 ? data : 'Binary data');
                
                updateTimeout();

                if (data.startsWith('START:')) {
                    const totalPackets = parseInt(data.split(':')[1], 16);
                    console.log(`🎬 Starting transfer with ${totalPackets} packets`);
                    this.requestNextPacket(currentPacket);
                } else if (data.startsWith('END:')) {
                    console.log('🏁 Transfer complete');
                    this.transferTimestamp = parseInt(data.split(':')[1]);
                    console.log(`📅 Transfer timestamp: ${this.transferTimestamp}`);
                    cleanup();
                    resolve(completeData);
                } else {
                    const dataView = new Uint8Array(receivedData.buffer).slice(4);
                    completeData = concatenateArrays(completeData, dataView);
                    currentPacket++;
                    this.requestNextPacket(currentPacket);
                }
            };
        });
    }

    async requestNextPacket(packetNumber) {
        const packetHex = packetNumber.toString(16).padStart(4, '0');
        try {
            await this.core.queue.enqueue(async () => {
                await this.core.characteristics.dataTransfer.writeValue(
                    new TextEncoder().encode(packetHex)
                );
            }, `Request Next Packet: ${packetHex}`);
        } catch (error) {
            console.error('Error requesting next packet:', error);
            throw error;
        }
    }

    handleDataTransferNotification(event) {
        const value = event.target.value;
        if (this.dataTransferCallback) {
            this.dataTransferCallback(value);
        } else {
            console.log('🚨 No data transfer callback registered');
        }
    }

    parseBinaryWifiNetworks(binaryData) {
        const networks = [];
        const dataView = new DataView(binaryData.buffer);
        let offset = 0;

        while (offset < binaryData.length) {
            const mac = BleDataTransfer.readMacAddress(dataView, offset);
            offset += BleDataTransfer.MAC_ADDR_SIZE;

            const ssid = BleDataTransfer.readFixedString(dataView, offset, BleDataTransfer.SSID_SIZE);
            offset += BleDataTransfer.SSID_SIZE;

            const rssi = BleDataTransfer.readInt8(dataView, offset);
            offset += BleDataTransfer.RSSI_SIZE;

            const channel = BleDataTransfer.readUint8(dataView, offset);
            offset += BleDataTransfer.CHANNEL_SIZE;

            const type = BleDataTransfer.readFixedString(dataView, offset, BleDataTransfer.TYPE_SIZE);
            offset += BleDataTransfer.TYPE_SIZE;

            const lastSeen = BleDataTransfer.readUint64(dataView, offset);
            offset += BleDataTransfer.TIMESTAMP_SIZE;

            const timesSeen = BleDataTransfer.readUint32(dataView, offset);
            offset += BleDataTransfer.COUNTER_SIZE;

            networks.push({
                ssid,
                mac,
                rssi,
                channel,
                type,
                last_seen: lastSeen,
                times_seen: timesSeen
            });
        }

        return { ssids: networks, timestamp: this.transferTimestamp };
    }

    parseBinaryWifiDevices(binaryData) {
        const devices = [];
        const dataView = new DataView(binaryData.buffer);
        let offset = 0;

        while (offset < binaryData.length) {
            const mac = BleDataTransfer.readMacAddress(dataView, offset);
            offset += BleDataTransfer.MAC_ADDR_SIZE;

            const bssid = BleDataTransfer.readMacAddress(dataView, offset);
            offset += BleDataTransfer.MAC_ADDR_SIZE;

            const rssi = BleDataTransfer.readInt8(dataView, offset);
            offset += BleDataTransfer.RSSI_SIZE;

            const channel = BleDataTransfer.readUint8(dataView, offset);
            offset += BleDataTransfer.CHANNEL_SIZE;

            const lastSeen = BleDataTransfer.readUint64(dataView, offset);
            offset += BleDataTransfer.TIMESTAMP_SIZE;

            const timesSeen = BleDataTransfer.readUint32(dataView, offset);
            offset += BleDataTransfer.COUNTER_SIZE;

            devices.push({
                mac,
                bssid,
                rssi,
                channel,
                last_seen: lastSeen,
                times_seen: timesSeen
            });
        }

        return { stations: devices, timestamp: this.transferTimestamp };
    }

    parseBinaryBleDevices(binaryData) {
        const devices = [];
        const dataView = new DataView(binaryData.buffer);
        let offset = 0;

        while (offset < binaryData.length) {
            const mac = BleDataTransfer.readMacAddress(dataView, offset);
            offset += BleDataTransfer.MAC_ADDR_SIZE;

            const name = BleDataTransfer.readFixedString(dataView, offset, BleDataTransfer.NAME_SIZE);
            offset += BleDataTransfer.NAME_SIZE;

            const rssi = BleDataTransfer.readInt8(dataView, offset);
            offset += BleDataTransfer.RSSI_SIZE;

            const lastSeen = BleDataTransfer.readUint64(dataView, offset);
            offset += BleDataTransfer.TIMESTAMP_SIZE;

            const isPublic = BleDataTransfer.readUint8(dataView, offset) !== 0;
            offset += BleDataTransfer.IS_PUBLIC_SIZE;

            const timesSeen = BleDataTransfer.readUint32(dataView, offset);
            offset += BleDataTransfer.COUNTER_SIZE;

            devices.push({
                mac,
                name,
                rssi,
                last_seen: lastSeen,
                is_public: isPublic,
                times_seen: timesSeen
            });
        }

        return { ble_devices: devices, timestamp: this.transferTimestamp };
    }

    async requestWifiNetworks() {
        try {
            console.log('📥 Requesting WiFi networks...');
            const dataPromise = this.receivePackets();
            await this.core.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode('ssid_list')
            );
            const data = await dataPromise;
            return this.parseBinaryWifiNetworks(data);
        } catch (error) {
            throw new Error(`Error requesting WiFi networks: ${error.message}`);
        }
    }

    async requestWifiDevices() {
        try {
            console.log('📥 Requesting WiFi devices...');
            const dataPromise = this.receivePackets();
            await this.core.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode('client_list')
            );
            const data = await dataPromise;
            return this.parseBinaryWifiDevices(data);
        } catch (error) {
            throw new Error(`Error requesting WiFi devices: ${error.message}`);
        }
    }

    async requestBleDevices() {
        try {
            console.log('📥 Requesting BLE devices...');
            const dataPromise = this.receivePackets();
            await this.core.characteristics.dataTransfer.writeValue(
                new TextEncoder().encode('ble_list')
            );
            const data = await dataPromise;
            return this.parseBinaryBleDevices(data);
        } catch (error) {
            throw new Error(`Error requesting BLE devices: ${error.message}`);
        }
    }
}
