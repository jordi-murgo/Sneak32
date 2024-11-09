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

            // Update UI with firmware / status info
            this.uiService.updateDeviceInfo(event.detail);

            // Get the setup page component
            const setupPage = document.querySelector('sneak-setup');
            
            // Check if setupPage is defined
            if (setupPage && typeof setupPage.handleDeviceSettingsLoad === 'function') {
                // If settings are available in the connection event, update them
                if (event.detail.settings) {
                    setupPage.handleDeviceSettingsLoad(event.detail.settings);
                }
            } else {
                console.error('Setup page not found or handleDeviceSettingsLoad is not a function');
            }

            this.uiService.showMainContent();
        });

        document.addEventListener('device-disconnected', () => {
            console.log('📱 Device disconnected, showing connect page');
            this.uiService.showConnectPage();
        });

        // Escuchar el evento de actualización del estado del dispositivo
        document.addEventListener('device-status-update', (event) => {
            console.log('📱 Device status update received:', event.detail);
            this.uiService.updateDeviceStatus({ deviceStatus: event.detail.status }); // Actualizar el estado
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