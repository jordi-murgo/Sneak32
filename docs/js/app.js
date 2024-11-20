import { BleService } from './services/ble.service.js';
import { UiService } from './services/ui.service.js';

class App {
    constructor() {
        console.log('🚨 App constructor');
        this.bleService = new BleService();
        this.uiService = new UiService();
    }

    initialize() {
        this.uiService.initializeUI();
        this.setupEventListeners();
        this.bleService.setConnectionStateCallback(this.handleConnectionStateChange.bind(this));

        // Mostrar página de conexión inicialmente
        this.uiService.showConnectPage();
    }

    // Eventos del Menu
    setupEventListeners() {
        document.addEventListener('page-change', (e) => {
            this.handlePageChange(e.detail.page);
        });

        document.addEventListener('save-device-data', () => {
            this.handleSaveData();
        });

        document.addEventListener('save-wifi-networks', () => {
            this.handleSaveWifiNetworks();
        });

        document.addEventListener('save-wifi-devices', () => {
            this.handleSaveWifiDevices();
        });

        document.addEventListener('save-ble-devices', () => {
            this.handleSaveBleDevices();
        });


        document.addEventListener('clear-device-data', () => {
            this.handleClearData();
        });

        document.addEventListener('reset-device', () => {
            this.handleResetDevice();
        });

        document.addEventListener('disconnect-device', () => {
            this.handleDisconnect();
        });

        document.addEventListener('device-connected', async (event) => {
            console.log('📱 Device connected event received:', event.detail);
            this.uiService.showMainContent();
        });

        document.addEventListener('device-disconnected', () => {
            console.log('📱 Device disconnected, showing connect page');
            this.uiService.showConnectPage();
        });

        document.addEventListener('settings-updated', async (event) => {
            const settings = event.detail;
            try {
                await this.bleService.updateSettings(settings);
                console.log('Settings saved successfully');
            } catch (error) {
                console.error('Error saving settings:', error);
            }
        });

        document.addEventListener('load-wifi-networks', async () => {
            console.log('🎯 App :: Event :: Loading WiFi networks');
            await this.loadWifiNetworks();
        });

        document.addEventListener('load-wifi-devices', async () => {
            console.log('🎯 App :: Event :: Loading WiFi devices');
            await this.loadWifiDevices();
        });

        document.addEventListener('load-ble-devices', async () => {
            console.log('🎯 App :: Event :: Loading BLE devices');
            await this.loadBleDevices();
        });

        document.addEventListener('connection-requested', async () => {
            // Notify connection attempt starting
            document.dispatchEvent(new CustomEvent('connection-starting', {
                detail: { timestamp: Date.now() }
            }));

            try {
                const connectionInfo = await this.bleService.connect().catch(error => {
                    if (error.name === 'NotFoundError') {
                        console.log('👤 User cancelled the device selection');
                        return null;
                    }
                    document.dispatchEvent(new CustomEvent('connection-error', {
                        detail: { message: error.message }
                    }));
                    return null;
                });

                if (!connectionInfo) {
                    console.log('❌ No connection info received');
                    return;
                }

                console.log('✅ Connected successfully:', connectionInfo);

                // Dispatch initial connection event to show device-info page with loading state
                document.dispatchEvent(new CustomEvent('device-connected', {
                    detail: connectionInfo
                }));

                // Start loading device info
                document.dispatchEvent(new CustomEvent('device-info-loading'));

                // Fetch complete device information
                const deviceInfo = await this.bleService.fetchDeviceInfo();

                // Update device info with complete information
                document.dispatchEvent(new CustomEvent('device-info-loaded', {
                    detail: {
                        ...connectionInfo,
                        ...deviceInfo
                    }
                }));

                await new Promise(resolve => setTimeout(resolve, 100));
                await this.loadWifiNetworks();
                await new Promise(resolve => setTimeout(resolve, 100));
                await this.loadWifiDevices();
                await new Promise(resolve => setTimeout(resolve, 100));
                await this.loadBleDevices();

                // Ok, we can start notifications for device status
                this.bleService.startStatusNotifications();

            } catch (error) {
                console.error('❌ Initialization error:', error);
            }
        });


        document.addEventListener('send-command', async (event) => {
            this.executeCommand(event.detail.command);
        });

        document.addEventListener('offline-view-requested', () => {
            this.handlePageChange('Device Info');
            this.uiService.showMainContent();
        });
    }

    async executeCommand(command) {
        try {
            const response = await this.bleService.sendCommand(command);
            document.dispatchEvent(new CustomEvent('command-response', {
                detail: {
                    command: command,
                    response,
                    success: true,
                    timestamp: Date.now()
                }
            }));
            return response;
        } catch (error) {
            document.dispatchEvent(new CustomEvent('command-response', {
                detail: {
                    command: command,
                    response: error.message,
                    success: false,
                    timestamp: Date.now()
                }
            }));
            throw error;
        }
    }

    handlePageChange(page) {
        console.log('🔄 Page change:', page);
        this.uiService.updatePage(page);
        document.getElementById('pageTitle').textContent = page;
    }

    handleModeChange(mode) {
        console.log('🔄 Mode change:', mode);
        // Handle mode change logic
    }

    async handleSaveData() {
        console.log('💾 Saving data...');
        try {
            const result =await this.bleService.saveData();
            await this.uiService.showToast(result, {
                color: 'success'
            });
        } catch (error) {
            await this.uiService.showToast('Error saving data', {
                color: 'danger'
            });
            console.error('Error saving data:', error);
        }
    }

    async handleSaveWifiNetworks() {
        console.log('💾 Saving WiFi networks...');
        try {
            const result = await this.bleService.saveWifiNetworks();
            await this.uiService.showToast(result, {
                color: 'success'
            });
        } catch (error) {
            await this.uiService.showToast('Error saving WiFi networks', {
                color: 'danger'
            });
            console.error('Error saving WiFi networks:', error);
        }
    }

    async handleSaveWifiDevices() {
        console.log('💾 Saving WiFi devices...');
        try {
            const result = await this.bleService.saveWifiDevices();
            await this.uiService.showToast(result, {
                color: 'success'
            });
        } catch (error) {
            await this.uiService.showToast('Error saving WiFi devices', {
                color: 'danger'
            });
            console.error('Error saving WiFi devices:', error);
        }
    }

    async handleSaveBleDevices() {
        console.log('💾 Saving BLE devices...');
        try {
            const result = await this.bleService.saveBleDevices();
            await this.uiService.showToast(result, {
                color: 'success'
            });
        } catch (error) {
            await this.uiService.showToast('Error saving BLE devices', {
                color: 'danger'
            });
            console.error('Error saving BLE devices:', error);
        }
    }

    handleClearData() {
        console.log('🔄 Clearing data...');
        this.bleService.clearData();
    }

    handleResetDevice() {
        console.log('🔄 Resetting device...');
        this.bleService.resetDevice();
    }

    handleDisconnect() {
        console.log('🔄 Disconnecting...');
        this.bleService.disconnect();
        this.uiService.showConnectPage();
    }

    handleConnectionStateChange(isConnected) {
        console.log('🔌 Connection state changed:', isConnected);
        if (isConnected) {
            this.uiService.showMainContent();
        } else {
            this.uiService.showConnectPage();
        }
    }

    async loadWifiDevices() {
        document.dispatchEvent(new CustomEvent('wifi-devices-loading'));
        try {
            const wifiDeviceList = await this.bleService.requestWifiDevices();
            document.dispatchEvent(new CustomEvent('wifi-devices-loaded', { 
                detail: wifiDeviceList 
            }));
        } catch (error) {
            document.dispatchEvent(new CustomEvent('ble-service-error', { 
                detail: error 
            }));
        }
    }

    async loadBleDevices() {
        document.dispatchEvent(new CustomEvent('ble-devices-loading'));
        try {
            const bleDeviceList = await this.bleService.requestBleDevices();
            document.dispatchEvent(new CustomEvent('ble-devices-loaded', { 
                detail: bleDeviceList 
            }));
        } catch (error) {
            document.dispatchEvent(new CustomEvent('ble-service-error', { 
                detail: error 
            }));
        }
    }

    async loadWifiNetworks() {
        document.dispatchEvent(new CustomEvent('wifi-networks-loading'));
        try {
            const wifiNetworkList = await this.bleService.requestWifiNetworks();
            document.dispatchEvent(new CustomEvent('wifi-networks-loaded', { 
                detail: wifiNetworkList 
            }));
        } catch (error) {
            document.dispatchEvent(new CustomEvent('ble-service-error', { 
                detail: error 
            }));
        }
    }
}

// Initialize app
const app = new App();
app.initialize();