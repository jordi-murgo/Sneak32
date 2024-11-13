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

    setupEventListeners() {
        document.addEventListener('page-change', (e) => {
            this.handlePageChange(e.detail.page);
        });

        document.addEventListener('save-data', () => {
            this.handleSaveData();
        });

        document.addEventListener('clear-data', () => {
            this.handleClearData();
        });

        document.addEventListener('reset-device', () => {
            this.handleResetDevice();
        });

        document.addEventListener('disconnect', () => {
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
                document.dispatchEvent(new CustomEvent('wifi-networks-loading'));
                const wifiNetworkList = await this.bleService.requestWifiNetworks();
                document.dispatchEvent(new CustomEvent('wifi-networks-loaded', { detail: wifiNetworkList }));

                await new Promise(resolve => setTimeout(resolve, 100));
                document.dispatchEvent(new CustomEvent('wifi-devices-loading'));
                const wifiDeviceList = await this.bleService.requestWifiDevices();
                document.dispatchEvent(new CustomEvent('wifi-devices-loaded', { detail: wifiDeviceList }));

                await new Promise(resolve => setTimeout(resolve, 100));
                document.dispatchEvent(new CustomEvent('ble-devices-loading'));
                const bleDeviceList = await this.bleService.requestBleDevices();
                document.dispatchEvent(new CustomEvent('ble-devices-loaded', { detail: bleDeviceList }));

            } catch (error) {
                console.error('❌ Initialization error:', error);
            }
        });


        document.addEventListener('send-command', async (event) => {
            this.executeCommand(event.detail.command);
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
        this.uiService.updatePage(page);
        document.getElementById('pageTitle').textContent = page;
    }

    handleModeChange(mode) {
        // Handle mode change logic
    }

    handleSaveData() {
        console.log('Saving data...');
        // Implementar lógica de guardado
    }

    handleClearData() {
        console.log('Clearing data...');
        // Implementar lógica de limpieza
    }

    handleResetDevice() {
        console.log('Resetting device...');
        // Implementar lógica de reinicio
    }

    handleDisconnect() {
        console.log('Disconnecting...');
        // Implementar lógica de desconexión
    }

    handleConnectionStateChange(isConnected) {
        console.log('🔌 Connection state changed:', isConnected);
        if (isConnected) {
            this.uiService.showMainContent();
        } else {
            this.uiService.showConnectPage();
        }
    }
}

// Initialize app
const app = new App();
app.initialize();