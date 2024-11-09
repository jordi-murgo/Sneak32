import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakWifiNetworks extends LitElement {
    static properties = {
        networks: { type: Array },
        isScanning: { type: Boolean }
    };

    constructor() {
        super();
        this.networks = [];
        this.isScanning = false;
    }

    render() {
        return html`
            <div class="wifi-networks-content">
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>WiFi Networks</h2>
                            <p>Found: ${this.networks.length}</p>
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
                        this.renderNetworksList()
                    }
                </ion-list>
            </div>
        `;
    }

    renderNetworksList() {
        return this.networks.map(network => html`
            <ion-item>
                <ion-icon 
                    slot="start" 
                    name="wifi-outline"
                    style="color: ${this.getSignalColor(network.rssi)}"
                ></ion-icon>
                <ion-label>
                    <h3>${network.ssid}</h3>
                    <p>Channel: ${network.channel}</p>
                    <p>RSSI: ${network.rssi} dBm</p>
                </ion-label>
                <ion-badge slot="end">${network.encryption}</ion-badge>
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
        this.dispatchEvent(new CustomEvent('start-wifi-scan', {
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

        .wifi-networks-content {
            max-width: 800px;
            margin: 0 auto;
        }

        ion-spinner {
            margin-right: 1rem;
        }
    `;
}

customElements.define('sneak-wifi-networks', SneakWifiNetworks); 