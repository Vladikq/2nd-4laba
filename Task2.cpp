#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>

using namespace std;

// Класс для хранения данных о доставленных посылках
class Package {
public:
    string productCode; // Код товара
    string city;        // Населенный пункт
    string recipient;   // ФИО получателя

    Package(const string& code, const string& city, const string& recipient)
        : productCode(code), city(city), recipient(recipient) {}
};

// Функция для случайной генерации строки
string generateRandomString(size_t length, const string& chars) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dist(0, chars.size() - 1);

    string result;
    for (size_t i = 0; i < length; ++i) {
        result += chars[dist(gen)];
    }
    return result;
}

// Функция для генерации случайных данных о посылках
vector<Package> generateRandomPackages(size_t size) {
    static vector<string> cities = {
        "Москва", "Санкт-Петербург", "Новосибирск", "Екатеринбург", 
        "Казань", "Челябинск", "Самара", "Уфа", "Тюмень", "Томск"
    };

    static vector<string> surnames = {
        "Иванов", "Петров", "Сидоров", "Кузнецов", "Смирнов", 
        "Васильев", "Миронов", "Куликов", "Лебедев", "Федоров"
    };

    static vector<string> names = {
        "Иван", "Петр", "Сергей", "Андрей", "Дмитрий", 
        "Алексей", "Артем", "Василий", "Николай", "Анна"
    };

    static vector<string> middleNames = {
        "Иванович", "Петрович", "Сергеевич", "Андреевич", 
        "Дмитриевич", "Алексеевич", "Артемович", "Николаевич", "Павлович", "Евгеньевич"
    };

    vector<Package> packages;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> cityDist(0, cities.size() - 1);
    uniform_int_distribution<> surnameDist(0, surnames.size() - 1);
    uniform_int_distribution<> nameDist(0, names.size() - 1);
    uniform_int_distribution<> middleNameDist(0, middleNames.size() - 1);
    bernoulli_distribution codeTypeDist(0.7); // 70% вероятность для "SH"

    for (size_t i = 0; i < size; ++i) {
        string codePrefix = codeTypeDist(gen) ? "SH" : "AB";
        string productCode = codePrefix + generateRandomString(4, "0123456789");
        string city = cities[cityDist(gen)];
        string recipient = surnames[surnameDist(gen)] + " " + names[nameDist(gen)] + " " + middleNames[middleNameDist(gen)];
        packages.emplace_back(productCode, city, recipient);
    }
    return packages;
}

// Функция для поиска фамилий получателей в одном потоке
vector<string> findRecipientsSingleThread(const vector<Package>& packages, const regex& pattern) {
    vector<string> recipients;
    for (const auto& package : packages) {
        if (regex_match(package.productCode, pattern)) {
            string surname = package.recipient.substr(0, package.recipient.find(' '));
            recipients.push_back(surname);
        }
    }
    return recipients;
}

// Функция для поиска фамилий в многопоточном режиме
void findRecipientsMultiThread(const vector<Package>& packages, const regex& pattern, vector<string>& recipients, mutex& mtx, size_t start, size_t end) {
    vector<string> localRecipients;
    for (size_t i = start; i < end; ++i) {
        if (regex_match(packages[i].productCode, pattern)) {
            string surname = packages[i].recipient.substr(0, packages[i].recipient.find(' '));
            localRecipients.push_back(surname);
        }
    }
    lock_guard<mutex> lock(mtx);
    recipients.insert(recipients.end(), localRecipients.begin(), localRecipients.end());
}

int main() {
    // Параметры генерации данных
    size_t dataSize = 100; // Размер массива данных
    int numThreads = 5;       // Количество потоков

    // Генерация случайных данных
    vector<Package> packages = generateRandomPackages(dataSize);

    // Регулярное выражение
    string patternStr = "^SH.*$";
    regex pattern(patternStr);



    // Однопоточная обработка
    auto start = chrono::high_resolution_clock::now();
    vector<string> singleThreadRecipients = findRecipientsSingleThread(packages, pattern);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> singleThreadDuration = end - start;



    // Многопоточная обработка
    vector<thread> threads;
    vector<string> multiThreadRecipients;
    mutex mtx;

    size_t chunkSize = packages.size() / numThreads;
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < numThreads; ++i) {
        size_t startIdx = i * chunkSize;
        size_t endIdx = (i == numThreads - 1) ? packages.size() : (i + 1) * chunkSize;
        threads.emplace_back(findRecipientsMultiThread, ref(packages), ref(pattern), ref(multiThreadRecipients), ref(mtx), startIdx, endIdx);
    }

    for (auto& t : threads) {
        t.join();
    }
    end = chrono::high_resolution_clock::now();
    chrono::duration<double> multiThreadDuration = end - start;




    // Вывод результатов
    cout << "Размер данных: " << dataSize << " записей\n";
    cout << "Результаты однопоточной обработки:\n";
    cout << "Найдено фамилий: " << singleThreadRecipients.size() << "\n";
    cout << "Время однопоточной обработки: " << singleThreadDuration.count() << " секунд\n\n";

    cout << "Результаты многопоточной обработки:\n";
    cout << "Найдено фамилий: " << multiThreadRecipients.size() << "\n";
    cout << "Время многопоточной обработки: " << multiThreadDuration.count() << " секунд\n";

    return 0;
}
