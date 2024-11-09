import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakWifiStations extends LitElement {
    static properties = {
        stations: { type: Array },
        isScanning: { type: Boolean }
    };

    constructor() {
        super();
        this.stations = [];
        this.isScanning = false;
    }

    render() {
        return html`
            <div class="wifi-stations-content">
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>WiFi Stations</h2>
                            <p>Found: ${this.stations.length}</p>
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
                        this.renderStationsList()
                    }
                </ion-list>
            </div>
        `;
    }

    renderStationsList() {
        return this.stations.map(station => html`
            <ion-item>
                <ion-icon 
                    slot="start" 
                    name="phone-portrait-outline"
                    style="color: ${this.getSignalColor(station.rssi)}"
                ></ion-icon>
                <ion-label>
                    <h3>${station.mac}</h3>
                    <p>RSSI: ${station.rssi} dBm</p>
                    <p>Last seen: ${station.lastSeen}</p>
                </ion-label>
                <ion-badge slot="end">${station.vendor || 'Unknown'}</ion-badge>
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
        this.dispatchEvent(new CustomEvent('start-stations-scan', {
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

        .wifi-stations-content {
            max-width: 800px;
            margin: 0 auto;
        }

        ion-spinner {
            margin-right: 1rem;
        }
    `;
}

customElements.define('sneak-wifi-stations', SneakWifiStations); 