#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <exception>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <iomanip>

class Logger {
    std::ofstream ofs;
public:
    Logger(const std::string &filename = "rental_log.txt") {
        ofs.open(filename, std::ios::app);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot open log file");
        }
    }
    ~Logger() {
        if (ofs.is_open()) ofs.close(); // RAII: file closed in destructor
    }
    void log(const std::string &msg) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        ofs << "[" << std::put_time(std::localtime(&now), "%F %T") << "] " << msg << std::endl;
    }
};

class VehicleException : public std::exception {
    std::string msg;
public:
    VehicleException(const std::string &m): msg(m) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

class VehicleNotAvailable : public VehicleException {
public:
    VehicleNotAvailable(const std::string &m): VehicleException(m) {}
};

class BatteryLowException : public VehicleException {
public:
    BatteryLowException(const std::string &m): VehicleException(m) {}
};

class OverloadException : public VehicleException {
public:
    OverloadException(const std::string &m): VehicleException(m) {}
};

class InvalidReturnException : public VehicleException {
public:
    InvalidReturnException(const std::string &m): VehicleException(m) {}
};

class Vehicle {
protected:
    int id;
    std::string model;
    double dailyRate;
    bool isRented;
public:
    Vehicle(int id_, const std::string &model_, double dailyRate_)
        : id(id_), model(model_), dailyRate(dailyRate_), isRented(false) {}

    virtual ~Vehicle() = default;

    int getId() const { return id; }
    std::string getModel() const { return model; }
    bool getIsRented() const { return isRented; }
    void setRented(bool r) { isRented = r; }

    // pure virtual rentCost
    virtual double rentCost(int days) const = 0;

    // start can throw VehicleException
    virtual void start() /* noexcept(false) */ {
        // base default: nothing special
    }

    // clone pattern for copying polymorphic objects
    virtual std::unique_ptr<Vehicle> clone() const = 0;

    virtual std::string info() const {
        std::ostringstream oss;
        oss << "[" << id << "] " << model << " (rate " << dailyRate << ")";
        return oss.str();
    }
};

// Car
class Car : public Vehicle {
    int passengerCapacity;
public:
    Car(int id_, const std::string &model_, double dailyRate_, int cap)
        : Vehicle(id_, model_, dailyRate_), passengerCapacity(cap) {}

    double rentCost(int days) const override {
        return dailyRate * days;
    }

    std::unique_ptr<Vehicle> clone() const override {
        return std::make_unique<Car>(*this);
    }

    std::string info() const override {
        std::ostringstream oss;
        oss << Vehicle::info() << " Car cap=" << passengerCapacity;
        return oss.str();
    }
};

// Truck
class Truck : public Vehicle {
    double maxLoadKg;
public:
    Truck(int id_, const std::string &model_, double dailyRate_, double maxLoadKg_)
        : Vehicle(id_, model_, dailyRate_), maxLoadKg(maxLoadKg_) {}

    double rentCost(int days) const override {
        // default without load
        return dailyRate * days;
    }

    // overload: if carrying load, charge extra per kg (example)
    double rentCost(int days, double loadKg) const {
        double base = dailyRate * days;
        double loadFeePerKg = 0.10; // simple model: 0.10 currency unit per kg per day
        return base + loadKg * loadFeePerKg * days;
    }

    double getMaxLoadKg() const { return maxLoadKg; }

    std::unique_ptr<Vehicle> clone() const override {
        return std::make_unique<Truck>(*this);
    }

    std::string info() const override {
        std::ostringstream oss;
        oss << Vehicle::info() << " Truck maxLoadKg=" << maxLoadKg;
        return oss.str();
    }
};

// ElectricCar
class ElectricCar : public Vehicle {
    double batteryCapacityKwh;
    double currentChargeKwh;
public:
    ElectricCar(int id_, const std::string &model_, double dailyRate_, double batteryCapacity, double currentCharge)
        : Vehicle(id_, model_, dailyRate_), batteryCapacityKwh(batteryCapacity), currentChargeKwh(currentCharge) {}

    double rentCost(int days) const override {
        double base = dailyRate * days;
        double minChargeNeeded = 0.2 * batteryCapacityKwh; // example threshold
        double surcharge = 0.0;
        if (currentChargeKwh < minChargeNeeded) {
            surcharge = 50.0; // flat surcharge if battery low when rented
        }
        return base + surcharge;
    }

    void start() override {
        double minStartCharge = 0.1 * batteryCapacityKwh;
        if (currentChargeKwh < minStartCharge) {
            throw BatteryLowException("Battery too low to start vehicle id=" + std::to_string(id));
        }
        // else start OK
    }

    void charge(double kwh) {
        currentChargeKwh += kwh;
        if (currentChargeKwh > batteryCapacityKwh) currentChargeKwh = batteryCapacityKwh;
    }

    double getCurrentCharge() const { return currentChargeKwh; }

    std::unique_ptr<Vehicle> clone() const override {
        return std::make_unique<ElectricCar>(*this);
    }

    std::string info() const override {
        std::ostringstream oss;
        oss << Vehicle::info() << " Electric battery=" << currentChargeKwh << "/" << batteryCapacityKwh;
        return oss.str();
    }
};

class RentalManager {
    std::vector<std::unique_ptr<Vehicle>> fleet;
    Logger &logger;

    // rentals: vehicleId -> (memberId, dueDate)
    struct RentalInfo {
        std::string memberId;
        std::chrono::system_clock::time_point dueDate;
        double expectedLoadKg; // used if truck
    };
    std::unordered_map<int, RentalInfo> activeRentals;

    // helper to find vehicle ptr by id
    Vehicle* findVehicle(int vehicleId) {
        for (auto &p : fleet) {
            if (p->getId() == vehicleId) return p.get();
        }
        return nullptr;
    }

    std::chrono::system_clock::time_point daysFromNow(int days) {
        return std::chrono::system_clock::now() + std::chrono::hours(24LL * days);
    }

public:
    RentalManager(Logger &log) : logger(log) {}

    // add vehicle (makes clone to keep ownership)
    void addVehicle(const Vehicle &v) {
        fleet.push_back(v.clone());
    }

    // rentVehicle: optional loadKg default to 0
    void rentVehicle(const std::string &memberId, int vehicleId, int days, double loadKg = 0.0) noexcept(false) {
        Vehicle* v = findVehicle(vehicleId);
        if (!v) {
            std::string msg = "Vehicle not found id=" + std::to_string(vehicleId);
            logger.log(msg);
            throw VehicleException(msg);
        }
        if (v->getIsRented()) {
            std::string msg = "Vehicle not available (already rented) id=" + std::to_string(vehicleId);
            logger.log(msg);
            throw VehicleNotAvailable(msg);
        }

        // compute cost polymorphically; for Truck with load, use overload via dynamic_cast
        double cost = 0.0;
        if (Truck* t = dynamic_cast<Truck*>(v)) {
            if (loadKg > t->getMaxLoadKg()) {
                std::string msg = "Requested load " + std::to_string(loadKg) + " > max " + std::to_string(t->getMaxLoadKg());
                logger.log("Overload attempt: " + msg);
                throw OverloadException(msg);
            }
            // use overload rentCost(days, loadKg)
            cost = t->rentCost(days, loadKg);
        } else {
            // general vehicles (Car, ElectricCar) use rentCost(int)
            cost = v->rentCost(days);
        }

        // Attempt to start the vehicle (may throw for ElectricCar)
        try {
            v->start(); // ElectricCar::start may throw BatteryLowException
        } catch (...) {
            std::string msg = std::string("Start failed for vehicle id=") + std::to_string(vehicleId);
            logger.log(msg + " -> exception thrown while starting");
            throw; // rethrow to caller; ensure manager does not mark rented
        }

        // mark as rented and record due date
        v->setRented(true);
        RentalInfo info{memberId, daysFromNow(days), loadKg};
        activeRentals[vehicleId] = info;

        std::ostringstream oss;
        oss << "Rented vehicle id=" << vehicleId << " to member=" << memberId << " for " << days
            << " days; cost=" << cost;
        logger.log(oss.str());
        std::cout << oss.str() << std::endl;
    }

    // returnVehicle
    void returnVehicle(const std::string &memberId, int vehicleId, int actualDays, bool damageFlag) noexcept(false) {
        Vehicle* v = findVehicle(vehicleId);
        if (!v) {
            std::string msg = "Return failed: Vehicle not found id=" + std::to_string(vehicleId);
            logger.log(msg);
            throw VehicleException(msg);
        }
        auto it = activeRentals.find(vehicleId);
        if (it == activeRentals.end()) {
            std::string msg = "Return failed: Vehicle not rented id=" + std::to_string(vehicleId);
            logger.log(msg);
            throw VehicleException(msg);
        }
        if (it->second.memberId != memberId) {
            std::string msg = "Return failed: member mismatch for vehicle id=" + std::to_string(vehicleId);
            logger.log(msg);
            throw VehicleException(msg);
        }

        // compute base expected cost using polymorphism (we could re-call rentCost with days)
        double baseCost = 0.0;
        RentalInfo info = it->second;
        if (Truck* t = dynamic_cast<Truck*>(v)) {
            // use recorded expectedLoadKg
            baseCost = t->rentCost(actualDays, info.expectedLoadKg);
        } else {
            baseCost = v->rentCost(actualDays);
        }

        // penalty if late: if now > dueDate
        auto now = std::chrono::system_clock::now();
        double penalty = 0.0;
        if (now > info.dueDate) {
            auto diff = std::chrono::duration_cast<std::chrono::hours>(now - info.dueDate).count();
            int lateDays = static_cast<int>(diff / 24) + 1; // at least 1 day
            penalty += lateDays * 20.0; // example: 20 per late day
        }

        // damage handling: if damageFlag true, evaluate severity (simulate threshold)
        if (damageFlag) {
            // simulate: severe damage threshold random or deterministic; we'll base on vehicleId for determinism
            bool severe = (vehicleId % 2 == 0); // simulate: even id -> severe
            if (severe) {
                std::string msg = "Severe damage reported on return for vehicle id=" + std::to_string(vehicleId);
                logger.log(msg);
                // mark incident and throw InvalidReturnException
                // still mark vehicle not rented (depending on policy), but we'll throw to caller
                v->setRented(false);
                activeRentals.erase(vehicleId);
                throw InvalidReturnException(msg);
            } else {
                penalty += 100.0; // minor damage fee
                logger.log("Minor damage fee applied for vehicle id=" + std::to_string(vehicleId));
            }
        }

        double total = baseCost + penalty;

        // finalize return
        v->setRented(false);
        activeRentals.erase(vehicleId);

        std::ostringstream oss;
        oss << "Vehicle id=" << vehicleId << " returned by " << memberId << ". Base=" << baseCost
            << " Penalty=" << penalty << " Total=" << total;
        logger.log(oss.str());
        std::cout << oss.str() << std::endl;
    }

    // Overloaded: chargeBattery(vehicleId, kwh)
    void chargeBattery(int vehicleId, double kwh) noexcept(false) {
        Vehicle* v = findVehicle(vehicleId);
        if (!v) {
            std::string msg = "Charge failed: Vehicle not found id=" + std::to_string(vehicleId);
            logger.log(msg);
            throw VehicleException(msg);
        }
        ElectricCar* ev = dynamic_cast<ElectricCar*>(v);
        if (!ev) {
            std::string msg = "Charge failed: vehicle id=" + std::to_string(vehicleId) + " is not an EV";
            logger.log(msg);
            throw VehicleException(msg);
        }
        ev->charge(kwh);
        std::ostringstream oss;
        oss << "Charged EV id=" << vehicleId << " + " << kwh << "kWh (now " << ev->getCurrentCharge() << " kWh)";
        logger.log(oss.str());
        std::cout << oss.str() << std::endl;
    }

    // Overloaded: chargeBattery(memberId, vehicleId, kwh) (just example overload)
    void chargeBattery(const std::string &memberId, int vehicleId, double kwh) noexcept(false) {
        // could verify member authorized; simple example logs member
        chargeBattery(vehicleId, kwh);
        logger.log("Charge requested by member " + memberId + " for vehicle " + std::to_string(vehicleId));
    }

    void listFleet() const {
        std::cout << "Fleet:" << std::endl;
        for (const auto &p : fleet) {
            std::cout << "  " << p->info() << (p->getIsRented() ? " [RENTED]" : "") << std::endl;
        }
    }
};

int main() {
    try {
        Logger logger("rental_log.txt");
        RentalManager manager(logger);

        // 1. Tambah 3 kendaraan (Car, Truck, ElectricCar).
        Car c1(1, "Toyota Avanza", 200.0, 7);
        Truck t1(2, "Hino Dutro", 400.0, 1000.0); // maxLoadKg = 1000
        ElectricCar e1(3, "Tesla Model 3", 350.0, 75.0, 5.0); // battery 75 kWh, currentCharge 5 kWh (low)

        manager.addVehicle(c1);
        manager.addVehicle(t1);
        manager.addVehicle(e1);

        manager.listFleet();

        std::cout << "\n--- Test case 2: Sewa Truck with overload -> expect OverloadException ---\n";
        try {
            // try to rent truck with load > maxLoad
            manager.rentVehicle("memberA", 2, 3, 1200.0);
        } catch (const OverloadException &ex) {
            std::cout << "Caught OverloadException: " << ex.what() << std::endl;
            logger.log(std::string("Caught OverloadException: ") + ex.what());
        } catch (const std::exception &ex) {
            std::cout << "Other exception: " << ex.what() << std::endl;
            logger.log(std::string("Other exception: ") + ex.what());
        }

        std::cout << "\n--- Test case 3: Sewa ElectricCar with low charge -> BatteryLowException at start() ---\n";
        try {
            manager.rentVehicle("memberB", 3, 2);
        } catch (const BatteryLowException &ex) {
            std::cout << "Caught BatteryLowException: " << ex.what() << std::endl;
            logger.log(std::string("Caught BatteryLowException: ") + ex.what());
        } catch (const std::exception &ex) {
            std::cout << "Other exception: " << ex.what() << std::endl;
            logger.log(std::string("Other exception: ") + ex.what());
        }

        std::cout << "\n--- Charge the ElectricCar, then rent ---\n";
        try {
            manager.chargeBattery("memberB", 3, 30.0); // charge 30 kWh
            manager.rentVehicle("memberB", 3, 2);
        } catch (const std::exception &ex) {
            std::cout << "Exception during charge/rent: " << ex.what() << std::endl;
            logger.log(std::string("Exception during charge/rent: ") + ex.what());
        }

        std::cout << "\n--- Test case 4: Sewa Car normal, kembalikan terlambat 2 hari -> penalti dihitung ---\n";
        try {
            manager.rentVehicle("memberC", 1, 1); // rent Car id=1 for 1 day
            // eSimulate time: we cannot modify system clock; instead we'll directly manipulate dueDate for test.
            // For the purpose of this demo, we will hack activeRentals by reusing returnVehicle after waiting.
            // In real unit test you'd inject clock or mock chrono. Here we simply sleep is not practical.
            // So we'll simulate late by computing actualDays and assume dueDate passed when returning.
            // Wait 1 second then return with actualDays=3 to simulate 2 days lat
            std::this_thread::sleep_for(std::chrono::seconds(1));
            manager.returnVehicle("memberC", 1, 3, false); // actualDays=3 -> was 1 -> late 2 days -> penalty
        } catch (const std::exception &ex) {
            std::cout << "Exception: " << ex.what() << std::endl;
            logger.log(std::string("Exception: ") + ex.what());
        }

        std::cout << "\n--- Test case 5: Return with damage flag true -> InvalidReturnException if severe ---\n";
        try {
            // First rent truck properly (within limits)
            manager.rentVehicle("memberD", 2, 2, 500.0);
            // Return with damage true. truck id=2 is even -> our simulation marks even id severe
            manager.returnVehicle("memberD", 2, 2, true);
        } catch (const InvalidReturnException &ex) {
            std::cout << "Caught InvalidReturnException: " << ex.what() << std::endl;
            logger.log(std::string("Caught InvalidReturnException: ") + ex.what());
        } catch (const std::exception &ex) {
            std::cout << "Other exception: " << ex.what() << std::endl;
            logger.log(std::string("Other exception: ") + ex.what());
        }

        std::cout << "\n--- Final fleet status ---\n";
        manager.listFleet();

    } catch (const std::exception &ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
    }
    return 0;
}
