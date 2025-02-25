

#include <fstream>
#include <vector>

struct Measurement {
    std::string type; // "metric" or "imperial"
    double value;
};

// Function to save measurements to a .mis file
void saveMeasurements(const std::string& filename, const std::vector<Measurement>& measurements) {
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    for (const auto& measurement : measurements) {
        file << measurement.type << " " << measurement.value << "\n";
    }

    file.close();
}

// Function to load measurements from a .mis file
std::vector<Measurement> loadMeasurements(const std::string& filename) {
    std::vector<Measurement> measurements;
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return measurements;
    }

    Measurement measurement;
    while (file >> measurement.type >> measurement.value) {
        measurements.push_back(measurement);
    }

    file.close();
    return measurements;
}



int main() {
    // Example measurements
    std::vector<Measurement> measurements = {
        {"metric", 100.0},
        {"imperial", 39.37}
    };

    // Save measurements to a .mis file
    saveMeasurements("data.mis", measurements);

    // Load measurements from a .mis file
    std::vector<Measurement> loadedMeasurements = loadMeasurements("data.mis");

    // Print loaded measurements
    for (const auto& measurement : loadedMeasurements) {
        std::cout << "Type: " << measurement.type << ", Value: " << measurement.value << std::endl;
    }

    return 0;
}
