# Simulator-Rental-Kendaraan-dengan-Performa-dan-Fault-Handling
=================================================================================

Program ini merupakan sistem penyewaan kendaraan berbasis OOP dalam C++.
Fitur utama meliputi inheritance (Car, Truck, ElectricCar), exception handling 
(VehicleException, BatteryLowException, OverloadException, InvalidReturnException),
polymorphism runtime melalui virtual functions dan dynamic_cast,
serta file logging menggunakan RAII wrapper Logger.

Program mengelola armada kendaraan, proses sewa, pengembalian, perhitungan biaya,
penanganan muatan pada Truck, serta pengecekan baterai pada ElectricCar sebelum start.

---------------------------------------------------------------------------------
PEMBAGIAN TUGAS KELOMPOK
---------------------------------------------------------------------------------

• Vehicle classes dan cloning ➤ Hafizh: Vehicle base class, atribut umum, virtual function clone() dan info()

• Exceptions dan error policies ➤ Khansa: Implementasi Car, kapasitas penumpang, perhitungan biaya dasar termasuk validasi input

• Rental Manager dan transaction logic ➤ Judhis: Implementasi RentalManager: addVehicle(), rentVehicle(), returnVehicle(), map activeRentals, handling late penalty dan damage flag

• Logger dan persistence ➤ Dion: RAII Logger, file logging runtime, otomatis close di destruktor

• Testing, scenario scripts dan dokumentasi ➤ Octa: pembuatan scenarios.txt, hasil run, dan pelaporan

---------------------------------------------------------------------------------
FITUR DAN KONSEP OOP YANG DITERAPKAN
---------------------------------------------------------------------------------

- Inheritance: Car, Truck, ElectricCar mewarisi Vehicle.
- Polymorphism: Fungsi rentCost() dan info() dipanggil melalui pointer base.
- dynamic_cast: Mendeteksi tipe turunan saat runtime untuk membedakan biaya Truck.
- Exception hierarchy: VehicleException sebagai base, turunan khusus untuk tiap kondisi.
- RAII: Logger membuka file pada konstruktor dan menutup otomatis pada destruktor.
- Smart Pointer: std::unique_ptr digunakan untuk cloning fleet secara polimorfik.

---------------------------------------------------------------------------------
FILE DAN STRUKTUR KODE
---------------------------------------------------------------------------------

vehicle_rental.cpp (isi seluruh program)
rental_log.txt (hasil logging runtime)
Tidak membutuhkan library eksternal selain standard library C++17.

---------------------------------------------------------------------------------
CARA KOMPILASI
---------------------------------------------------------------------------------

Windows:
g++ -std=c++17 -O2 -o rental.exe vehicle_rental.cpp

Linux:
g++ -std=c++17 -O2 -o rental vehicle_rental.cpp

macOS:
clang++ -std=c++17 -O2 -o rental vehicle_rental.cpp

---------------------------------------------------------------------------------
CARA MENJALANKAN
---------------------------------------------------------------------------------

Windows:
rental.exe

Linux / macOS:
./rental

---------------------------------------------------------------------------------
CONTOH HASIL RUN (RINGKAS)
---------------------------------------------------------------------------------

Fleet:
[1] Toyota Avanza (rate 200) Car cap=7
[2] Hino Dutro (rate 400) Truck maxLoadKg=1000
[3] Tesla Model 3 (rate 350) Electric battery=5/75

--- Test Overload ---
Caught OverloadException: Requested load 1200 > max 1000

--- Test Battery Low ---
Caught BatteryLowException: Battery too low to start vehicle id=3

--- Charge and Rent ---
Charged EV id=3 +30kWh
Rented vehicle id=3 to memberB for 2 days

---------------------------------------------------------------------------------
LOGGING (RAII)
---------------------------------------------------------------------------------

Semua aktivitas penting ditulis ke rental_log.txt otomatis melalui Logger.
File ditutup otomatis melalui destruktor tanpa perlu manual close.

---------------------------------------------------------------------------------
CATATAN PENGGUNAAN DAN ASUMSI
---------------------------------------------------------------------------------

- Biaya sewa Truck bertambah berdasarkan muatan per hari.
- ElectricCar gagal start jika baterai di bawah threshold.
- Pengembalian dengan damage pada id genap dianggap severe dan memicu exception.
- Waktu keterlambatan dihitung berdasarkan actualDays.

---------------------------------------------------------------------------------
AKHIR DOKUMENTASI
---------------------------------------------------------------------------------
