export class UiService {
    constructor() {
        console.log('🚨 UiService constructor');
        this.currentPage = 'Device Info';
        this.components = {
            deviceInfo: null,
            sideMenu: null,
            pageTitle: null,
            pageContents: null
        };
        this.connectPage = null;
        this.mainContent = null;
    }

    initializeUI() {
        // Initialize component references
        this.components.deviceInfo = document.querySelector('sneak-device-info');
        this.components.sideMenu = document.querySelector('sneak-side-menu');
        this.components.pageTitle = document.querySelector('#pageTitle');
        this.components.pageContents = document.querySelectorAll('.page-content');

        this.connectPage = document.querySelector('#connectPage');
        this.mainContent = document.querySelector('#mainContent');
        
        // Setup event listeners
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Listen for UI events
        document.addEventListener('save-data', () => this.handleSaveData());
        document.addEventListener('clear-data', () => this.handleClearData());
        document.addEventListener('reset-device', () => this.handleResetDevice());
        document.addEventListener('disconnect', () => this.handleDisconnect());
        document.addEventListener('mode-change', () => this.handleModeChange());

        document.addEventListener('connection-error', (event) => {
            console.log('🚨 Connection error:', event.detail);
            this.showToast(event.detail.message + " Try to delete pairing information.", { duration: 5000, color: 'danger' });
        });

        document.addEventListener('ble-service-error', (event) => {
            console.log('🚨 BLE Service error:', event.detail);
            this.showToast(event.detail.message, { duration: 5000, color: 'danger' });
        });

    }

    updatePage(page) {
        // Update current page reference
        this.currentPage = page;
        
        // Update page title
        if (this.components.pageTitle) {
            this.components.pageTitle.textContent = page;
        }

        // Update side menu selection
        if (this.components.sideMenu) {
            this.components.sideMenu.currentPage = page;
        }

        // Show/hide appropriate content
        this.components.pageContents.forEach(content => {
            const shouldShow = content.tagName.toLowerCase() === `sneak-${page.toLowerCase().replace(' ', '-')}`;
            content.style.display = shouldShow ? 'block' : 'none';
        });

        // Dispatch page change event
        this.dispatchEvent('ui-page-changed', { page });
    }


    // Alert Handlers
    async showAlert(options) {
        const alert = document.createElement('ion-alert');
        Object.assign(alert, options);
        document.body.appendChild(alert);
        await alert.present();
        const result = await alert.onDidDismiss();
        alert.remove();
        return result;
    }

    async showToast(message, options = {}) {
        const toast = document.createElement('ion-toast');
        Object.assign(toast, {
            message,
            duration: 2000,
            position: 'bottom',
            ...options
        });
        document.body.appendChild(toast);
        await toast.present();
        const result = await toast.onDidDismiss();
        toast.remove();
        return result;
    }

    // Event Handlers
    async handleSaveData() {
        this.dispatchEvent('ui-save-data');
        await this.showToast('Data saved successfully');
    }

    async handleClearData() {
        const alert = await this.showAlert({
            header: 'Clear Data',
            message: 'Are you sure you want to clear all captured data?',
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'Clear',
                    role: 'destructive',
                    handler: () => {
                        this.dispatchEvent('ui-clear-data');
                    }
                }
            ]
        });
    }

    async handleResetDevice() {
        const alert = await this.showAlert({
            header: 'Reset Device',
            message: 'Are you sure you want to restart the device?',
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'Reset',
                    role: 'destructive',
                    handler: () => {
                        this.dispatchEvent('ui-reset-device');
                    }
                }
            ]
        });
    }

    async handleDisconnect() {
        const alert = await this.showAlert({
            header: 'Disconnect',
            message: 'Are you sure you want to disconnect from the device?',
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'Disconnect',
                    handler: () => {
                        this.dispatchEvent('ui-disconnect');
                    }
                }
            ]
        });
    }

    async handleModeChange() {
        const alert = await this.showAlert({
            header: 'Operation Mode',
            inputs: [
                {
                    type: 'radio',
                    label: 'Off Mode',
                    value: 'off',
                    checked: true
                },
                {
                    type: 'radio',
                    label: 'Monitor Mode',
                    value: 'monitor'
                },
                {
                    type: 'radio',
                    label: 'Active Mode',
                    value: 'active'
                }
            ],
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'OK',
                    handler: (mode) => {
                        this.dispatchEvent('ui-mode-change', { mode });
                    }
                }
            ]
        });
    }

    // Loading indicator methods
    async showLoading(message = 'Please wait...') {
        const loading = document.createElement('ion-loading');
        loading.message = message;
        document.body.appendChild(loading);
        await loading.present();
        return loading;
    }

    hideLoading(loading) {
        if (loading) {
            loading.dismiss();
            loading.remove();
        }
    }

    // Helper methods
    dispatchEvent(eventName, detail = {}) {
        const event = new CustomEvent(eventName, {
            detail,
            bubbles: true,
            composed: true
        });
        document.dispatchEvent(event);
    }

    showConnectPage() {
        if (this.connectPage && this.mainContent) {
            this.connectPage.classList.remove('hidden');
            this.mainContent.classList.add('hidden');
        }
    }

    showMainContent() {
        if (this.connectPage && this.mainContent) {
            this.connectPage.classList.add('hidden');
            this.mainContent.classList.remove('hidden');
        }
    }

}
