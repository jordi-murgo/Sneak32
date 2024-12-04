import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakBleDevices extends LitElement {
    static properties = {
        devices: { type: Array },
        isLoading: { type: Boolean },
        deviceStatus: { type: Object }
    };

    constructor() {
        super();
        this.devices = [];
        this.isLoading = false;
        this.deviceStatus = { bleDevices: 0 };

        document.addEventListener('ble-devices-loading', () => {
            console.log('🔵 BleDevices :: Devices loading');
            this.isLoading = true;
        });

        document.addEventListener('ble-devices-error', () => {
            console.log('🔵 BleDevices :: Devices error');
            this.isLoading = false;
        });

        document.addEventListener('ble-devices-loaded', async (event) => {
            console.log('🔵 BleDevices :: Devices loaded', event.detail);
            this.isLoading = false;
            this.devices = event.detail.ble_devices;

            this.localTimestamp = Date.now();
            this.remoteTimestamp = event.detail.timestamp * 1000;

            // Decorate devices with vendor and local_last_seen 
            for (const device of this.devices) {
                device.vendor = await window.vendorDecode(device.mac);
                device.local_last_seen = this.localTimestamp - this.remoteTimestamp + (device.last_seen * 1000);
            }
            
            console.log('🔵 BleDevices :: Decorated Devices', this.devices);

            this.requestUpdate();
        });

        document.addEventListener('device-status-update', (event) => {
            console.log('🔵 BleDevices :: Device status update', event.detail);
            this.updateDeviceStatus(event.detail.status);
        });
    }

    updateDeviceStatus(status) {
        this.deviceStatus = {
            ...this.deviceStatus,
            ...status
        };
        this.requestUpdate();
    }

    getLastSeenDate(lastSeen) {
        const date = new Date(lastSeen);

        var year = date.getFullYear();

        if (isNaN(year)) {
            return 'Unknown';
        }

        var month = ('0' + (date.getMonth() + 1)).slice(-2);
        var day = ('0' + date.getDate()).slice(-2);
        var hours = ('0' + date.getHours()).slice(-2);
        var minutes = ('0' + date.getMinutes()).slice(-2);
        var seconds = ('0' + date.getSeconds()).slice(-2);

        return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }

    render() {
        return html`
            <div class="ble-devices-content">
                ${this.isLoading ? html`
                    <div class="loading-overlay">
                        <ion-spinner></ion-spinner>
                        <ion-label>Loading...</ion-label>
                    </div>
                ` : ''}
                
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>BLE Devices</h2>
                            <p>Downloaded: ${this.devices.length}, Scanned: ${this.deviceStatus.bleDevices}</p>
                        </ion-label>
                    </ion-item>
                    <!-- Actions -->
                    <ion-item>
                        <ion-button 
                            @click=${this.startLoad}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="refresh-outline"></ion-icon>
                            Reload
                        </ion-button>
                        <ion-button   
                            @click=${this.downloadDevices}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="download-outline"></ion-icon>
                            Download
                        </ion-button>
                        <ion-button 
                            color="warning"
                            @click=${this.saveDevices}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="save-outline"></ion-icon>
                            Save
                        </ion-button>
                        <ion-button 
                            color="danger"
                            @click=${this.deleteDevices}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="trash-outline"></ion-icon>
                            Delete
                        </ion-button>
                    </ion-item>

                    ${this.renderDevicesList()}
                </ion-list>
            </div>
        `;
    }

    renderDevicesList() {
        const sortedDevices = [...this.devices].sort((a, b) => b.rssi - a.rssi);

        return sortedDevices.map(device => html`
            <ion-item-sliding>
                <ion-item>
                    <ion-icon 
                        slot="start" 
                        name="bluetooth-outline"
                        style="color: ${this.getSignalColor(device.rssi)}"
                    ></ion-icon>
                    <ion-label>
                        <h3>${device.name}</h3>
                        <p>MAC: ${device.mac } ${device.vendor ? `(Vendor: ${device.vendor})` : ''}</p>
                        <p>RSSI: ${device.rssi} dBm, Last seen: ${this.getLastSeenDate(device.local_last_seen)}</p>
                    </ion-label>
                    <div class="badge-container">
                        ${device.rssi <= -90 ? html`
                            <ion-badge color="warning">Weak</ion-badge>
                        ` : ''}
                        <ion-badge color="primary">${device.times_seen}</ion-badge>
                    </div>
                </ion-item>
                <ion-item-options side="end">
                    <ion-item-option color="danger" @click=${() => this.deleteDevice(device.mac)}>
                        <ion-icon slot="start" name="trash-outline"></ion-icon>
                        Delete
                    </ion-item-option>
                </ion-item-options>
            </ion-item-sliding>
        `);
    }

    getSignalColor(dBm) {
        if (dBm > -50) dBm = -50;
        if (dBm < -100) dBm = -100;
    
        let ratio = (-dBm - 50) / 50;
        let r, g, b;
    
        if (ratio <= 0.3333) {
            let segmentRatio = ratio / 0.3333;
            r = Math.round(255 * segmentRatio);
            g = 255;
            b = Math.round(255 * (1 - segmentRatio));
        } else if (ratio <= 0.6666) {
            let segmentRatio = (ratio - 0.3333) / 0.3333;
            r = 255;
            g = Math.round(255 * (1 - segmentRatio));
            b = 0;
        } else {
            let segmentRatio = (ratio - 0.6666) / 0.3334;
            r = Math.round(255 + (21 - 255) * segmentRatio);
            g = Math.round(0 + (210 - 0) * segmentRatio);
            b = Math.round(0 + (210 - 0) * segmentRatio);
        }
    
        r = Math.max(0, Math.min(255, r));
        g = Math.max(0, Math.min(255, g));
        b = Math.max(0, Math.min(255, b));
    
        return `rgb(${r},${g},${b})`;
    }

    downloadDevices() {
        // Prepare data to export
        const exportData = this.devices.map(device => ({
            ...device,
        }));
        
        // Create JSON content
        const jsonContent = JSON.stringify(exportData, null, 2);
        
        // Generate filename with current date/time
        const now = new Date();
        const timestamp = `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, '0')}${String(now.getDate()).padStart(2, '0')}-${String(now.getHours()).padStart(2, '0')}${String(now.getMinutes()).padStart(2, '0')}${String(now.getSeconds()).padStart(2, '0')}`;
        const filename = `ble-devices-${timestamp}.json`;

        // Create blob and download link
        const blob = new Blob([jsonContent], { type: 'application/json' });
        const url = window.URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.download = filename;
        
        // Trigger download
        document.body.appendChild(link);
        link.click();
        
        // Cleanup
        document.body.removeChild(link);
        window.URL.revokeObjectURL(url);
    }

    saveDevices() {
        this.dispatchEvent(new CustomEvent('save-ble-devices', {
            bubbles: true,
            composed: true
        }));
    }

    deleteDevices() {
        this.dispatchEvent(new CustomEvent('delete-all-ble-devices', {
            bubbles: true,
            composed: true
        }));
    }

    deleteDevice(mac) {
        this.devices = this.devices.filter(device => device.mac !== mac);
        this.dispatchEvent(new CustomEvent('delete-ble-device', {
            detail: { mac },
            bubbles: true,
            composed: true
        }));
    }

    startLoad() {
        this.dispatchEvent(new CustomEvent('load-ble-devices', {
            bubbles: true,
            composed: true
        }));
    }

    static styles = css`
        :host {
            display: block;
            padding: 2px 0;
        }

        .ble-devices-content {
            margin: 0 auto;
        }

        ion-spinner {
            margin-right: 1rem;
        }

        .badge-container {
            display: flex;
            align-items: center;
            gap: 5px;
            margin-left: 8px;
        }

        ion-badge {
            text-transform: capitalize;
            padding: 4px 8px;
        }

        ion-badge[color="success"] {
            background: var(--ion-color-success, #2dd36f);
            color: var(--ion-color-success-contrast, #ffffff);
        }

        ion-badge[color="primary"] {
            background: var(--ion-color-primary, #3880ff);
            color: var(--ion-color-primary-contrast, #ffffff);
        }

        ion-badge[color="warning"] {
            background: var(--ion-color-warning, #ffc409);
            color: var(--ion-color-warning-contrast, #000000);
        }

        ion-item-option[color="danger"] {
            --background: var(--ion-color-danger);
            --color: var(--ion-color-danger-contrast);
            --ion-color-base: var(--ion-color-danger);
            --ion-color-contrast: var(--ion-color-danger-contrast);
        }

        ion-item-option ion-icon {
            margin-right: 8px;
            font-size: 1.2em;
        }

        ion-badge {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            min-height: 20px;
            min-width: 20px;
            border-radius: 4px;
        }

        ion-item-option {
            --button-color: var(--color);
        }

        ion-button {
            --padding-start: 12px;
            --padding-end: 12px;
            --padding-top: 8px;
            --padding-bottom: 8px;
        }

        ion-button[color="warning"] {
            --ion-color-base: var(--ion-color-warning, #ffc409);
            --ion-color-contrast: var(--ion-color-warning-contrast, #000000);
        }

        ion-button[color="danger"] {
            --ion-color-base: var(--ion-color-danger, #eb445a);
            --ion-color-contrast: var(--ion-color-danger-contrast, #ffffff);
        }

        .loading-overlay {
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(0, 0, 0, 0.8);
            padding: 20px;
            border-radius: 8px;
            z-index: 9999;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 10px;
        }

        .loading-overlay ion-spinner {
            --color: white;
            margin: 0;
        }

        .loading-overlay ion-label {
            color: white;
            margin: 0;
        }
    `;
}

customElements.define('sneak-ble-devices', SneakBleDevices);
