import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakWifiNetworks extends LitElement {
    static properties = {
        networks: { type: Array },
        isLoading: { type: Boolean },
        showActionSheet: { type: Boolean }
    };

    constructor() {
        console.log('🥁 WifiNetworks :: Constructor');
        super();
        this.networks = [];
        this.isLoading = false;
        this.showActionSheet = false;
        this.deviceStatus = { wifiNetworks: 0 };
        this.stations = {};

        document.addEventListener('wifi-networks-loading', () => {
            console.log('🥁 WifiNetworks :: WiFi networks loading');
            this.isLoading = true;
        });

        document.addEventListener('wifi-networks-loaded', async (event) => {
            console.log('🥁 WifiNetworks :: WiFi networks loaded', event.detail);

            this.isLoading = false;
            this.networks = event.detail.ssids;
            this.localTimestamp = Date.now();
            this.remoteTimestamp = event.detail.timestamp * 1000;

            // Decorate networks with local_last_seen and vendor using Promise.all
            for(const network of this.networks) {
                network.vendor = await window.vendorDecode(network.mac);
                network.local_last_seen = this.localTimestamp - this.remoteTimestamp + (network.last_seen * 1000);
            }

            this.requestUpdate();
        });

        document.addEventListener('wifi-devices-loading', () => {
            console.log('🥁 WifiNetworks :: WiFi devices loading');
            this.isLoading = true;
        });

        document.addEventListener('wifi-devices-error', () => {
            console.log('🔥 WifiNetworks :: WiFi devices error loading');
            this.isLoading = true;
        });

        document.addEventListener('wifi-devices-loaded', (event) => {
            console.log('🥁 WifiNetworks :: WiFi devices loaded', event.detail);
            this.isLoading = false;
            this.stations = [];

            for(const station of event.detail.stations) {
                this.stations[station.bssid] = 1 + (this.stations[station.bssid] || 0);
            }

            this.requestUpdate();
        });

        document.addEventListener('device-status-update', (event) => {
            console.log('🥁 DeviceInfo :: Device status update', event.detail);
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
            <div class="wifi-networks-content">
                ${this.isLoading ? html`
                    <div class="loading-overlay">
                        <ion-spinner></ion-spinner>
                        <ion-label>Loading...</ion-label>
                    </div>
                ` : ''}
                
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>WiFi Networks</h2>
                            <p>Downloaded: ${this.networks.length}, Scanned: ${this.deviceStatus.wifiNetworks}</p>
                        </ion-label>
                    </ion-item>
                    <!-- Actions -->
                    <ion-item>
                        <ion-button 
                            @click=${this.reloadNetworks}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="refresh-outline"></ion-icon>
                            Reload
                        </ion-button>
                        <ion-button   
                            @click=${this.downloadNetworks}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="download-outline"></ion-icon>
                            Download
                        </ion-button>
                        <ion-button 
                            color="warning"
                            @click=${this.saveNetworks}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="save-outline"></ion-icon>
                            Save
                        </ion-button>
                        <ion-button 
                            color="danger"
                            @click=${this.deleteNetworks}
                            ?disabled=${this.isLoading}
                        >
                            <ion-icon slot="start" name="trash-outline"></ion-icon>
                            Delete
                        </ion-button>
                    </ion-item>

                    ${this.renderNetworksList()}
                </ion-list>

                <ion-action-sheet
                    .isOpen=${this.showActionSheet}
                    .header=${'Delete Options'}
                    .buttons=${[
                {
                    text: `Delete Irrelevant Networks (${this.getIrrelevantNetworks().length})`,
                    role: 'destructive',
                    handler: () => {
                        this.getIrrelevantNetworks().forEach(network =>
                            this.deleteNetwork(network.ssid)
                        );
                    }
                },
                {
                    text: `Delete Weak Networks (${this.getWeakNetworks().length})`,
                    role: 'destructive',
                    handler: () => {
                        this.getWeakNetworks().forEach(network =>
                            this.deleteNetwork(network.ssid)
                        );
                    }
                },
                {
                    text: 'Cancel',
                    role: 'cancel'
                }
            ]}
                    @didDismiss=${() => this.showActionSheet = false}
                ></ion-action-sheet>
            </div>
        `;
    }

    renderNetworksList() {
        const sortedNetworks = [...this.networks].sort((a, b) => {
            // Primero comparar por RSSI (señal más fuerte primero)
            if (a.rssi !== b.rssi) {
                return b.rssi - a.rssi;
            }

            // Si el RSSI es igual, comparar por times_seen
            if (a.times_seen !== b.times_seen) {
                return b.times_seen - a.times_seen;
            }

            // Si times_seen es igual, comparar por last_seen
            return new Date(b.last_seen) - new Date(a.last_seen);
        });

        sortedNetworks.forEach(network => {
            if (network.mac !== 'FF:FF:FF:FF:FF:FF' && network.mac !== '00:00:00:00:00:00') {
                network.clients = this.stations[network.mac] - 1 || 0;
            }
        });

        return sortedNetworks.map(network => html`
            <ion-item-sliding>
                <ion-item>
                    <ion-icon 
                        slot="start" 
                        name="wifi-outline"
                        style="color: ${this.getSignalColor(network.rssi)};"
                    ></ion-icon>
                    <ion-label>
                        <h3>${network.ssid}</h3>
                        ${network.type != 'probe' ? html`<p>MAC: ${network.mac} ${network.vendor ? `(Vendor: ${network.vendor})` : ''}</p>` : ''}   
                        <p>${network.type === 'probe' ? 'Probe Request' : 'Channel: ' + network.channel}, RSSI: ${network.rssi} dBm</p>
                        <p>Last seen: ${this.getLastSeenDate(network.local_last_seen)}</p>
                    </ion-label>
                    <div class="badge-container">
                        ${network.rssi <= -90 ? html`
                            <ion-badge color="warning">Weak</ion-badge>
                        ` : ''}
                        ${network.clients > 0 ? html`<ion-badge color="success">${network.clients} clients</ion-badge>` : ''}
                        <ion-badge color="${network.type === 'probe' ? 'success' : 'primary'}">
                            ${network.type} ${network.times_seen ? `: ${network.times_seen}` : ''}
                        </ion-badge>
                    </div>
                </ion-item>
                <ion-item-options side="end">
                    <ion-item-option color="danger" @click=${() => this.deleteNetwork(network.ssid)}>
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

    reloadNetworks() {
        this.isLoading = true;
        this.dispatchEvent(new CustomEvent('load-wifi-networks', {
            bubbles: true,
            composed: true
        }));

        // Simulate scan completion after 5 seconds
        setTimeout(() => {
            this.isLoading = false;
        }, 5000);
    }

    saveNetworks() {
        this.dispatchEvent(new CustomEvent('save-wifi-networks', {
            bubbles: true,
            composed: true
        }));
    }

    deleteNetwork(ssid) {
        this.networks = this.networks.filter(network => network.ssid !== ssid);
        this.dispatchEvent(new CustomEvent('delete-network', {
            detail: { ssid },
            bubbles: true,
            composed: true
        }));
    }

    getIrrelevantNetworks() {
        return this.networks.filter(network => network.type === 'beacon' && network.times_seen <= 2);
    }

    getWeakNetworks() {
        return this.networks.filter(network => network.rssi <= -85);
    }

    downloadNetworks() {
        // Prepare data to export
        const exportData = this.networks.map(network => ({
            ...network,
            clients: network.mac !== 'FF:FF:FF:FF:FF:FF' && network.mac !== '00:00:00:00:00:00'
                ? (this.stations[network.mac] - 1 || 0)
                : 0,
            last_seen_formatted: this.getLastSeenDate(network.last_seen),
        }));

        // Create JSON content
        const jsonContent = JSON.stringify(exportData, null, 2);

        // Generate filename with current date/time
        const now = new Date();
        const timestamp = `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, '0')}${String(now.getDate()).padStart(2, '0')}-${String(now.getHours()).padStart(2, '0')}${String(now.getMinutes()).padStart(2, '0')}${String(now.getSeconds()).padStart(2, '0')}`;
        const filename = `wifi-networks-${timestamp}.json`;

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

    static styles = css`
        :host {
            display: block;
            padding: 2px 0;
        }

        .wifi-networks-content {
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

customElements.define('sneak-wifi-networks', SneakWifiNetworks);
