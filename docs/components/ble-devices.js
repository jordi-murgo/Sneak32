import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakBleDevices extends LitElement {
    static properties = {
        devices: { type: Array },
        isScanning: { type: Boolean }
    };

    constructor() {
        super();
        this.devices = [];
        this.isScanning = false;
    }

    render() {
        return html`
            <div class="ble-devices-content">
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>BLE Devices</h2>
                            <p>Found: ${this.devices.length}</p>
                        </ion-label>
                        <ion-button 
                            slot="end"
                            @click=${this.startScan}
                            ?disabled=${this.isScanning}
                        >
                            <ion-icon slot="start" name="refresh-outline"></ion-icon>
                            Scan
                        </ion-button>
                    </ion-item>

                    ${this.isScanning ? 
                        html`
                            <ion-item>
                                <ion-spinner></ion-spinner>
                                <ion-label>Scanning...</ion-label>
                            </ion-item>
                        ` : 
                        this.renderDevicesList()
                    }
                </ion-list>
            </div>
        `;
    }

    renderDevicesList() {
        return this.devices.map(device => html`
            <ion-item>
                <ion-icon 
                    slot="start" 
                    name="bluetooth-outline"
                    style="color: ${this.getSignalColor(device.rssi)}"
                ></ion-icon>
                <ion-label>
                    <h3>${device.name || device.mac}</h3>
                    <p>RSSI: ${device.rssi} dBm</p>
                    <p>Last seen: ${device.lastSeen}</p>
                </ion-label>
                <ion-badge slot="end">${device.vendor || 'Unknown'}</ion-badge>
            </ion-item>
        `);
    }

    getSignalColor(rssi) {
        if (rssi >= -50) return 'var(--ion-color-success)';
        if (rssi >= -65) return 'var(--ion-color-warning)';
        return 'var(--ion-color-danger)';
    }

    startScan() {
        this.isScanning = true;
        this.dispatchEvent(new CustomEvent('start-ble-scan', {
            bubbles: true,
            composed: true
        }));

        // Simulate scan completion after 5 seconds
        setTimeout(() => {
            this.isScanning = false;
        }, 5000);
    }

    static styles = css`
        :host {
            display: block;
            padding: 1rem;
        }

        .ble-devices-content {
            max-width: 800px;
            margin: 0 auto;
        }

        ion-spinner {
            margin-right: 1rem;
        }
    `;
}

customElements.define('sneak-ble-devices', SneakBleDevices); 