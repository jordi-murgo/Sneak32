import { LitElement, html, css } from 'https://cdn.jsdelivr.net/gh/lit/dist@3.2.1/core/lit-core.min.js';
import * as echarts from 'https://cdn.jsdelivr.net/npm/echarts@5.5.1/dist/echarts.esm.js';

export class SneakDataDashboard extends LitElement {
    static properties = {
        data: { type: Array },
        isLoading: { type: Boolean },
        wifiDevices: { type: Array },
        bleDevices: { type: Array },
        networks: { type: Array }
    };

    constructor() {
        super();
        this.data = [];
        this.isLoading = false;
        this.wifiDevices = [];
        this.bleDevices = [];
        this.networks = [];
        this.charts = {};

        // Add event listeners
        document.addEventListener('wifi-devices-loaded', (event) => {
            this.wifiDevices = event.detail.stations;
            this.updateCharts();
        });

        document.addEventListener('wifi-networks-loaded', (event) => {
            this.networks = event.detail.ssids;
            this.updateCharts();
        });

        document.addEventListener('ble-devices-loaded', (event) => {
            this.bleDevices = event.detail.ble_devices;
            this.updateCharts();
        });

        this.visibilityObserver = new IntersectionObserver(
            (entries) => {
                entries.forEach(entry => {
                    if (entry.isIntersecting) {
                        this.initializeChartsWhenVisible();
                    }
                });
            },
            { threshold: 0.1 }
        );

        this.resizeObserver = new ResizeObserver(entries => {
            for (const entry of entries) {
                if (entry.contentRect.width > 0 && entry.contentRect.height > 0) {
                    this.updateCharts();
                }
            }
        });
    }

    connectedCallback() {
        super.connectedCallback();
        this.visibilityObserver.observe(this);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        this.visibilityObserver.unobserve(this);
        this.resizeObserver.disconnect();
    }

    async initializeChartsWhenVisible() {
        await this.updateComplete;
        
        const chartsContainer = this.shadowRoot.querySelector('.charts-container');
        if (chartsContainer) {
            this.resizeObserver.observe(chartsContainer);
        }
        
        this.initializeCharts();
    }

    initializeCharts() {
        const devicesChartEl = this.shadowRoot.getElementById('devicesChart');
        const networkPacketsChartEl = this.shadowRoot.getElementById('networkPacketsChart');
        const probePacketsChartEl = this.shadowRoot.getElementById('probePacketsChart');
        const rssiPacketsChartEl = this.shadowRoot.getElementById('rssiPacketsChart');
        const bleRssiChartEl = this.shadowRoot.getElementById('bleRssiChart');
        const channelChartEl = this.shadowRoot.getElementById('channelChart');

        if (!devicesChartEl || !networkPacketsChartEl || !probePacketsChartEl || 
            !rssiPacketsChartEl || !bleRssiChartEl || !channelChartEl) {
            console.warn('Chart elements not found');
            return;
        }

        // Dispose previous instances
        if (this.charts.devicesChart) this.charts.devicesChart.dispose();
        if (this.charts.networkPacketsChart) this.charts.networkPacketsChart.dispose();
        if (this.charts.probePacketsChart) this.charts.probePacketsChart.dispose();
        if (this.charts.rssiPacketsChart) this.charts.rssiPacketsChart.dispose();
        if (this.charts.bleRssiChart) this.charts.bleRssiChart.dispose();
        if (this.charts.channelChart) this.charts.channelChart.dispose();

        // Initialize new instances
        this.charts.devicesChart = echarts.init(devicesChartEl);
        this.charts.networkPacketsChart = echarts.init(networkPacketsChartEl);
        this.charts.probePacketsChart = echarts.init(probePacketsChartEl);
        this.charts.rssiPacketsChart = echarts.init(rssiPacketsChartEl);
        this.charts.bleRssiChart = echarts.init(bleRssiChartEl);
        this.charts.channelChart = echarts.init(channelChartEl);
        
        this.updateCharts();

        // Agregar listener para redimensionar
        window.addEventListener('resize', () => {
            Object.values(this.charts).forEach(chart => chart?.resize());
        });
    }

    updateCharts() {
        if (!this.charts.devicesChart || !this.charts.channelChart || 
            !this.charts.networkPacketsChart || !this.charts.rssiPacketsChart ||
            !this.charts.bleRssiChart) {
            return;
        }

        // Devices Count chart
        const blePublic = this.bleDevices.filter(device => device.is_public === true).length;
        const blePrivate = this.bleDevices.filter(device => device.is_public !== true).length;
        
        const wifiBeacons = this.networks.filter(network => network.type !== "probe").length;
        const wifiProbes = this.networks.filter(network => network.type === "probe").length;
        
        const wifiDevicesProbing = this.wifiDevices.filter(device => 
            device.bssid === '00:00:00:00:00:00' || 
            device.bssid === 'FF:FF:FF:FF:FF:FF'
        ).length;
        const wifiDevicesConnected = this.wifiDevices.filter(device => 
            device.bssid !== '00:00:00:00:00:00' && 
            device.bssid !== 'FF:FF:FF:FF:FF:FF' &&
            device.bssid !== device.mac
        ).length;
        const wifiDevicesAp = this.wifiDevices.filter(device => 
            device.bssid === device.mac
        ).length;

        const devicesOption = {
            title: {
                text: 'Data Captured',
                left: 'center'
            },
            tooltip: {
                trigger: 'axis',
                axisPointer: {
                    type: 'shadow'
                }
            },
            legend: {
                top: 30
            },
            grid: {
                left: '3%',
                right: '4%',
                bottom: '3%',
                top: '70px',
                containLabel: true
            },
            xAxis: {
                type: 'category',
                data: ['BLE Devices', 'WiFi Networks', 'WiFi Devices']
            },
            yAxis: {
                type: 'value'
            },
            series: [

                {
                    name: 'Items',
                    type: 'bar',
                    stack: 'pila',
                    data: [blePublic, wifiBeacons, wifiDevicesConnected]
                },
                {
                    name: 'APs',
                    type: 'bar',
                    stack: 'pila',
                    data: [0, 0, wifiDevicesAp]
                },
                {
                    name: 'Random',
                    type: 'bar',
                    stack: 'pila',
                    data: [blePrivate, 0, 0]
                },      
                {
                    name: 'ProbeReqs',
                    type: 'bar',
                    stack: 'pila',
                    data: [0, wifiProbes, wifiDevicesProbing]
                },          
            ]
        };

        // Update channel distribution chart
        const channelDevices = {};
        const channelAPs = {};
        const channelTimesSeen = {};
        
        const validWifiDevices = this.wifiDevices.filter(device => 
            device.bssid !== 'FF:FF:FF:FF:FF:FF' && 
            device.bssid !== '00:00:00:00:00:00'
        );

        // Initialize all channels from 1 to 14 with zero
        for (let i = 1; i <= 14; i++) {
            channelDevices[i] = 0;
            channelAPs[i] = 0;
            channelTimesSeen[i] = 0;
        }

        // Count actual data
        validWifiDevices.forEach(device => {
            channelDevices[device.channel] += (device.bssid !== device.mac ? 1 : 0);
            channelAPs[device.channel] += (device.bssid === device.mac ? 1 : 0);
            channelTimesSeen[device.channel] += device.times_seen;
        });

        const channels = Array.from({length: 14}, (_, i) => i + 1); // [1,2,3,...,14]

        const channelOption = {
            title: {
                text: 'WiFi Channel Distribution',
                left: 'center'
            },
            tooltip: {
                trigger: 'axis',
                axisPointer: {
                    type: 'cross'
                }
            },
            legend: {
                data: ['APs', 'Devices', 'Packets'],
                top: 30
            },
            xAxis: {
                type: 'category',
                data: channels,
                axisTick: {
                    alignWithLabel: true
                }
            },
            grid: {
                left: '10%',
                right: '10%',
                top: '60px',
                bottom: '60px'
            },
            yAxis: [
                {
                    type: 'value',
                    name: 'Devices',
                    position: 'left',
                    axisLabel: {
                        margin: 16
                    }
                },
                {
                    type: 'value',
                    name: 'Packets',
                    position: 'right',
                    axisLabel: {
                        margin: 16
                    }
                }
            ],
            series: [
                {
                    name: 'Devices',
                    data: channels.map(channel => channelDevices[channel]),
                    type: 'bar',
                    stack: 'pila',
                    showBackground: true,
                    backgroundStyle: {
                        color: 'rgba(180, 180, 180, 0.2)'
                    }
                },
                {
                    name: 'APs',
                    data: channels.map(channel => channelAPs[channel]),
                    type: 'bar',
                    stack: 'pila',
                    showBackground: true,
                    backgroundStyle: {
                        color: 'rgba(180, 180, 180, 0.2)'
                    }
                },
                {
                    name: 'Packets',
                    data: channels.map(channel => channelTimesSeen[channel]),
                    type: 'line',
                    yAxisIndex: 1,
                    smooth: true,
                    lineStyle: {
                        width: 3
                    },
                    symbol: 'circle',
                    symbolSize: 8
                }
            ]
        };

        // Filter to keep only beacon networks
        const beaconNetworks = this.networks.filter(network => network.type === 'beacon');

        // Create network packets distribution chart
        const totalPackets = beaconNetworks.reduce((sum, network) => sum + network.times_seen, 0);
        const threshold = totalPackets * 0.03; // Cambiado a 0.03 (3%)

        // Separate networks into significant (>1%) and others
        const significantNetworks = [];
        let othersTotal = 0;

        beaconNetworks
            .sort((a, b) => b.times_seen - a.times_seen)
            .forEach(network => {
                if (network.times_seen > threshold) {
                    significantNetworks.push({
                        name: network.ssid || '<Hidden SSID>',
                        value: network.times_seen
                    });
                } else {
                    othersTotal += network.times_seen;
                }
            });

        // Add "Others" category if there are minor networks
        if (othersTotal > 0) {
            significantNetworks.push({
                name: 'Others (<3%)',
                value: othersTotal
            });
        }

        const networkPacketsOption = {
            title: {
                text: 'Network Packets Distribution',
                left: 'center'
            },
            tooltip: {
                trigger: 'item',
                formatter: '{a} <br/>{b}: {c} packets ({d}%)'
            },
            legend: {
                type: 'scroll',
                orient: 'vertical',
                right: 10,
                top: 40,
                bottom: 20
            },
            series: [
                {
                    name: 'Network Packets',
                    type: 'pie',
                    radius: ['40%', '70%'],
                    avoidLabelOverlap: true,
                    itemStyle: {
                        borderRadius: 10,
                        borderColor: '#fff',
                        borderWidth: 2
                    },
                    label: {
                        show: true,
                        formatter: '{b}: {d}%'
                    },
                    emphasis: {
                        label: {
                            show: true,
                            fontSize: 14,
                            fontWeight: 'bold'
                        }
                    },
                    data: significantNetworks
                }
            ]
        };

        // Create Probe Requests packets distribution chart
        const probeNetworks = this.networks.filter(network => network.type == 'probe');
        const totalProbePackets = probeNetworks.reduce((sum, network) => sum + network.times_seen, 0);
        const probeThreshold = totalProbePackets * 0.03; // Cambiado a 0.03 (3%)

        // Separate probe requests into significant (>1%) and others
        const significantProbes = [];
        let probeOthersTotal = 0;

        probeNetworks
            .sort((a, b) => b.times_seen - a.times_seen)
            .forEach(network => {
                if (network.times_seen > probeThreshold) {
                    significantProbes.push({
                        name: network.ssid || '<Null Probe>',
                        value: network.times_seen
                    });
                } else {
                    probeOthersTotal += network.times_seen;
                }
            });

        // Add "Others" category if there are minor networks
        if (probeOthersTotal > 0) {
            significantProbes.push({
                name: 'Others (<3%)',
                value: probeOthersTotal
            });
        }

        const probePacketsOption = {
            title: {
                text: 'Probe Requests Distribution',
                left: 'center'
            },
            tooltip: {
                trigger: 'item',
                formatter: '{a} <br/>{b}: {c} packets ({d}%)'
            },
            legend: {
                type: 'scroll',
                orient: 'vertical',
                right: 10,
                top: 40,
                bottom: 20
            },
            series: [
                {
                    name: 'Probe Packets',
                    type: 'pie',
                    radius: ['40%', '70%'],
                    avoidLabelOverlap: true,
                    itemStyle: {
                        borderRadius: 10,
                        borderColor: '#fff',
                        borderWidth: 2
                    },
                    label: {
                        show: true,
                        formatter: '{b}: {d}%'
                    },
                    emphasis: {
                        label: {
                            show: true,
                            fontSize: 14,
                            fontWeight: 'bold'
                        }
                    },
                    data: significantProbes
                }
            ]
        };

        // Create RSSI vs Packets scatter plot
        const rssiPacketsData = beaconNetworks.map(network => ({
            name: network.ssid || '<Hidden SSID>',
            value: [network.rssi, network.times_seen]
        }));

        const rssiPacketsOption = {
            title: {
                text: 'WiFi Signal Strength vs Packets',
                left: 'center'
            },
            tooltip: {
                trigger: 'item',
                formatter: function(params) {
                    return `${params.data.name}<br/>` +
                           `RSSI: ${params.data.value[0]} dBm<br/>` +
                           `Packets: ${params.data.value[1]}`;
                }
            },
            grid: {
                left: '10%',
                right: '5%',
                top: '60px',
                bottom: '60px'
            },
            xAxis: {
                type: 'value',
                name: 'RSSI (dBm)',
                nameLocation: 'middle',
                nameGap: 30,
                max: -20,
                min: -100
            },
            yAxis: {
                type: 'log',
                name: 'Packets Seen',
                nameLocation: 'middle',
                nameGap: 50,
                axisLabel: {
                    margin: 16
                }
            },
            series: [{
                name: 'Networks',
                type: 'scatter',
                symbolSize: function(data) {
                    // Tamaño del punto basado en el número de paquetes (logarítmico)
                    return Math.max(10, Math.log10(data[1]) * 10);
                },
                data: rssiPacketsData,
                emphasis: {
                    focus: 'series',
                    label: {
                        show: true,
                        formatter: function(param) {
                            return param.data.name;
                        },
                        position: 'top'
                    }
                }
            }]
        };

        // Create BLE RSSI vs Packets scatter plot
        const bleRssiData = this.bleDevices.map(device => ({
            name: device.name || device.mac,
            value: [device.rssi, device.times_seen],
            itemStyle: {
                color: device.is_public === true ? '#5470c6' : '#91cc75'
            }
        }));

        const bleRssiOption = {
            title: {
                text: 'BLE Signal Strength vs Packets',
                left: 'center'
            },
            tooltip: {
                trigger: 'item',
                formatter: function(params) {
                    return `${params.data.name}<br/>` +
                           `RSSI: ${params.data.value[0]} dBm<br/>` +
                           `Packets: ${params.data.value[1]}`;
                }
            },
            grid: {
                left: '10%',
                right: '5%',
                top: '60px',
                bottom: '60px'
            },
            xAxis: {
                type: 'value',
                name: 'RSSI (dBm)',
                nameLocation: 'middle',
                nameGap: 30,
                max: -20,
                min: -100
            },
            yAxis: {
                type: 'log',
                name: 'Packets Seen',
                nameLocation: 'middle',
                nameGap: 50,
                axisLabel: {
                    margin: 16
                }
            },
            series: [{
                name: 'Devices',
                type: 'scatter',
                symbolSize: function(data) {
                    return Math.max(10, Math.log10(data[1]) * 10);
                },
                data: bleRssiData,
                emphasis: {
                    focus: 'series',
                    label: {
                        show: true,
                        formatter: function(param) {
                            return param.data.name;
                        },
                        position: 'top'
                    }
                }
            }]
        };

        // Set chart options
        this.charts.devicesChart?.setOption(devicesOption);
        this.charts.networkPacketsChart?.setOption(networkPacketsOption);
        this.charts.probePacketsChart?.setOption(probePacketsOption);
        this.charts.rssiPacketsChart?.setOption(rssiPacketsOption);
        this.charts.bleRssiChart?.setOption(bleRssiOption);
        this.charts.channelChart?.setOption(channelOption);

        // Resize all charts
        Object.values(this.charts).forEach(chart => chart?.resize());
    }

    render() {

        return html`
            <div class="data-dashboard-content">
                <div class="charts-container">
                    <div id="devicesChart" class="chart"></div>
                    <div id="networkPacketsChart" class="chart"></div>
                    <div id="probePacketsChart" class="chart"></div>
                    <div id="rssiPacketsChart" class="chart"></div>
                    <div id="bleRssiChart" class="chart"></div>
                    <div id="channelChart" class="chart"></div>
                </div>

                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>Captured Data</h2>
                            <p>Total records: ${this.data.length}</p>
                        </ion-label>
                    </ion-item>

                    ${this.isLoading ? 
                        html`
                            <ion-item>
                                <ion-spinner></ion-spinner>
                                <ion-label>Loading data...</ion-label>
                            </ion-item>
                        ` : 
                        this.renderDataList()
                    }
                </ion-list>

                <div class="button-container">
                    <ion-button expand="block" @click=${this.downloadData}>
                        <ion-icon slot="start" name="download-outline"></ion-icon>
                        Download Data
                    </ion-button>
                </div>
            </div>
        `;
    }

    renderDataList() {
        return this.data.map(item => html`
            <ion-item>
                <ion-label>
                    <h3>${item.type}</h3>
                    <p>${item.timestamp}</p>
                    <p>${item.details}</p>
                </ion-label>
            </ion-item>
        `);
    }

    downloadData() {
        const dataStr = JSON.stringify(this.data, null, 2);
        const blob = new Blob([dataStr], { type: 'application/json' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `sneak32-data-${new Date().toISOString()}.json`;
        a.click();
        window.URL.revokeObjectURL(url);
    }

    static styles = css`
        :host {
            display: block;
            padding: 1rem;
        }

        .data-dashboard-content {
            margin: 0 auto;
        }

        .button-container {
            margin-top: 2rem;
        }

        ion-spinner {
            margin-right: 1rem;
        }

        .charts-container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 2rem;
            margin: 2rem 0;
            width: 100%;
        }

        .chart {
            height: 350px;
            min-width: 300px;
            background: var(--ion-background-color);
            border-radius: 8px;
            padding: 1rem;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
    `;
}

customElements.define('sneak-data-dashboard', SneakDataDashboard); 