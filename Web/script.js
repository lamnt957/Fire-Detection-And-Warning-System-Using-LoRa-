// Firebase Configuration
const firebaseConfig = {
    apiKey: "AIzaSyCcpykPQLjbXYPFwOswESYGP8Fi79gO-cM",
    authDomain: "doan-95b6b.firebaseapp.com",
    databaseURL: "https://doan-95b6b-default-rtdb.firebaseio.com",
    projectId: "doan-95b6b",
    storageBucket: "doan-95b6b.appspot.com",
    messagingSenderId: "545857800437",
    appId: "1:545857800437:web:5faa765232556168db1dbc",
    measurementId: "G-2JZ34XP55Q"
  };

// Initialize Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();

let map;
let markers = [];

// Initialize Google Map
function initMap() {
    map = new google.maps.Map(document.getElementById("google-map"), {
        center: { lat: 11.6434 , lng: 106.2845 }, // Default center (Hà Nội)
        zoom: 8,
    });
    fetchDevices();
}

// Fetch devices from Firebase
function fetchDevices() {
    const devicesRef = database.ref("/");
    devicesRef.on("value", (snapshot) => {
        const devices = snapshot.val();
        if (!devices) {
            console.error("No data found in Firebase.");
            return;
        }

        const deviceArray = Object.entries(devices).map(([node, device]) => ({
            id: node,
            ...device,
        }));
        updateDeviceList(deviceArray);
        updateMap(deviceArray);
        updateCurrentStatus(deviceArray); 
    });
}


// Update device list in the HTML
function updateDeviceList(devices) {
    const deviceListElement = document.getElementById("device-list");
    deviceListElement.innerHTML = ""; // Clear existing list

    devices.forEach((device) => {
        const listItem = document.createElement("li");
        listItem.className = "device-item";

        listItem.innerHTML = `
            <span class="device-name">Thiết bị ${device.id}</span>
            <span class="device-address">Lat: ${device.lat}, Lng: ${device.lng}</span>
            <button class="show-details">Xem chi tiết</button>
            <div class="device-details">
                <p>Nhiệt độ: ${device.temp}°C</p>
                <p>Độ ẩm: ${device.hum}%</p>
                <p>Flame: ${device.flame}</p>
                <p>Gas: ${device.gas}</p>
            </div>
        `;

        const button = listItem.querySelector(".show-details");
        button.addEventListener("click", () => {
            listItem.classList.toggle("active");
            if (listItem.classList.contains("active")) {
                button.textContent = "Ẩn bớt";
            } else {
                button.textContent = "Xem chi tiết";
            }
        });

        deviceListElement.appendChild(listItem);
    });
}
// Update current status based on flame values
function updateCurrentStatus(devices) {
    const statusBox = document.getElementById("current-status");
    const fireDevices = devices.filter(device => device.flame === "1"); // Lọc các thiết bị có flame = 1

    if (fireDevices.length === 0) {
        // Không có cháy
        statusBox.textContent = "Tình trạng hiện tại: Bình thường";
        statusBox.classList.remove("warning");
    } else {
        // Có cháy, liệt kê các thiết bị có cháy
        const fireDeviceIds = fireDevices.map(device => `Thiết bị ${device.id}`).join(", ");
        statusBox.textContent = `Tình trạng hiện tại: Có cháy ở ${fireDeviceIds}`;
        statusBox.classList.add("warning");
    }
}
// Update Google Map with devices
function updateMap(devices) {
    // Remove old markers
    markers.forEach((marker) => marker.setMap(null));
    markers = [];

    // Add new markers
    devices.forEach((device) => {
        if (!device.lat || !device.lng) return;

        const marker = new google.maps.Marker({
            position: { lat: device.lat, lng: device.lng },
            map,
            title: `Thiết bị ${device.id}`,
        });

        const infoWindow = new google.maps.InfoWindow({
            content: `
                <div>
                    <h3>Thiết bị ${device.id}</h3>
                    <p>Nhiệt độ: ${device.temp}°C</p>
                    <p>Độ ẩm: ${device.hum}%</p>
                    <p>Flame: ${device.flame}</p>
                    <p>Gas: ${device.gas}</p>
                </div>
            `,
        });

        marker.addListener("click", () => {
            infoWindow.open(map, marker);
        });

        markers.push(marker);
    });
}
