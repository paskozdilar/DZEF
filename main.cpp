/**
 * DZEF = DZenita's Encoding Format
 *
 * Specification:
 *
 * 	EXPRESSION = 
 *		Type::INT_32, NAME, INT_32 |
 *		Type::UINT_32, NAME, UINT_32 |
 *		Type::BOOLEAN, NAME, BOOLEAN |
 *		Type::FLOAT, NAME, FLOAT |
 *		Type::STRING, NAME, STRING |
 *		Type::STRUCT, NAME, { EXPRESSION }, type::STRUCT_END
 *
 * 	NAME = STRING
 * 	STRING = STRING_SIZE, STRING_VALUE
 * 	FLOAT = EXPONENT, MANTISSA
 *
 *	Type::INT_32 = 0
 *      Type::UINT_32 = 1
 *      Type::BOOLEAN = 2
 *      Type::FLOAT = 3
 *      Type::STRING = 4
 *      Type::STRUCT = 5
 *      Type::STRUCT_END = 5
 *
 * 	SIZE:
 * 		TYPE = 1 byte
 * 		INT_32 = 4 bytes big endian
 * 		UINT_32 = 4 bytes big endian
 * 		BOOLEAN = 1 byte
 * 		EXPONENT = 1 byte
 * 		MANTISSA = 3 byte
 * 		STRING_SIZE = 4 bytes
 * 		STRING_VALUE = STRING_SIZE bytes
 */

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>


namespace DZEF {

enum class Type {
	INT_32 = 0,
	UINT_32 = 1,
	BOOLEAN = 2,
	FLOAT = 3,
	STRING = 4,
	STRUCT = 5,
	STRUCT_END = 6
};


class Encoder {	
public:
	Encoder() = delete;
	Encoder(const Encoder&) = delete;

	Encoder(std::ostream &out) : out(out) { }

	void addNamedInt32(std::string name, long value) {
		out.put(static_cast<char>(Type::INT_32));
		addString(name);
		addIntBytes<4>(value);
	}

	void addNamedUInt32(std::string name, unsigned long value) {
		out.put(static_cast<char>(Type::UINT_32));
		addString(name);
		addIntBytes<4>(value);
	}

	void addNamedBoolean(std::string name, bool value) {
		out.put(static_cast<char>(Type::BOOLEAN));
		addString(name);
		out.put(value ? 0x01 : 0x00);
	}

	void addNamedFloat(std::string name, double value) {
		out.put(static_cast<char>(Type::FLOAT));
		addString(name);
		addFloat(static_cast<float>(value));
	}

	void addNamedString(std::string name, std::string value) {
		out.put(static_cast<char>(Type::STRING));
		addString(name);
		addString(value);
	}

	void beginStructure(std::string name) {
		out.put(static_cast<char>(Type::STRUCT));
		addString(name);
	}

	void endStructure() {
		out.put(static_cast<char>(Type::STRUCT_END));
	}

private:
	std::ostream &out;

	void addString(std::string s) {
		size_t n = s.length();
		addIntBytes<4>(n);
		out.write(s.c_str(), n);
	}

	template <int N, typename T>
	void addIntBytes(T value) {
		// Network Byte Order = big endian
		char buf[N] = {0};
		for (int i = 0; i < N; i++) {
			buf[N-i-1] = value & 0xff;
			value >>= 8;
		}
		out.write(buf, N);
	}

	void addFloat(float value) {
		// Use frexpf for portability
		int exp;
		float frac = frexpf(value, &exp);
		int64_t mant = static_cast<int64_t>(frac * (int64_t(1LL) << 24)); // scale to 24 bits
		addIntBytes<1>(exp);
		addIntBytes<3>(mant);
	}
};

class Decoder {
public:
    Decoder(std::istream &in) : in(in) {}

    void decode() {
        while (in.peek() != EOF) {
            Type type = static_cast<Type>(in.get());

            std::string name;
	    if (type != Type::STRUCT_END) {
		name = readString();
	    }

            switch (type) {
                case Type::INT_32:
                    std::cout << name << " = " << readInt32() << " (int32)\n";
                    break;
                case Type::UINT_32:
                    std::cout << name << " = " << readUInt32() << " (uint32)\n";
                    break;
                case Type::BOOLEAN:
                    std::cout << name << " = " << (in.get() ? "true" : "false") << " (boolean)\n";
                    break;
                case Type::FLOAT: {
                    int exp = in.get();
                    int mant = readIntBytes<3>();
                    float value = ldexp(static_cast<float>(mant) / (1 << 24), exp);
                    std::cout << name << " = " << value << " (float)\n";
                    break;
                }
                case Type::STRING: {
                    std::string value = readString();
                    std::cout << name << " = \"" << value << "\" (string)\n";
                    break;
                }
                case Type::STRUCT: {
                    std::cout << name << " (struct) {\n";
                    decode();  // recurse
                    std::cout << "} // " << name << "\n";
                    break;
                }
                case Type::STRUCT_END:
                    return; // exit the current structure
                default:
                    std::cerr << "Unknown type\n";
                    return;
            }
        }
    }

private:
    std::istream &in;

    std::string readString() {
        size_t size = readIntBytes<4>();
        std::string result(size, '\0');
        in.read(&result[0], size);
        return result;
    }

    int32_t readInt32() {
        return readIntBytes<4>();
    }

    uint32_t readUInt32() {
        return static_cast<uint32_t>(readIntBytes<4>());
    }

    template <int N>
    int32_t readIntBytes() {
        int32_t value = 0;
        for (int i = 0; i < N; ++i) {
            value = (value << 8) | in.get();
        }
        return value;
    }
};

}


int main() {
    std::ofstream outFile("test.bin", std::ios::binary);
    auto enc = DZEF::Encoder(outFile);

    std::string name;
    int choice;
    bool addMore = true;

    std::cout << "Welcome to DZEF CLI Encoder!\n";

    while (addMore) {
        std::cout << "Choose type: 0=Int32, 1=UInt32, 2=Boolean, 3=Float, 4=String, 5=Struct, 6=EndStruct\n> ";
        std::cin >> choice;

        if (choice == static_cast<int>(DZEF::Type::STRUCT_END)) {
            enc.endStructure();
        } else if (choice == static_cast<int>(DZEF::Type::STRUCT)) {
            std::cout << "Enter structure name: ";
            std::cin >> name;
            enc.beginStructure(name);
        } else {
            std::cout << "Enter name: ";
            std::cin >> name;

            switch (static_cast<DZEF::Type>(choice)) {
                case DZEF::Type::INT_32: {
                    long v; std::cout << "Int32 value: "; std::cin >> v;
                    enc.addNamedInt32(name, v);
                    break;
                }
                case DZEF::Type::UINT_32: {
                    unsigned long v; std::cout << "UInt32 value: "; std::cin >> v;
                    enc.addNamedUInt32(name, v);
                    break;
                }
                case DZEF::Type::BOOLEAN: {
                    bool v; std::cout << "Boolean value (0/1): "; std::cin >> v;
                    enc.addNamedBoolean(name, v);
                    break;
                }
                case DZEF::Type::FLOAT: {
                    double v; std::cout << "Float value: "; std::cin >> v;
                    enc.addNamedFloat(name, v);
                    break;
                }
                case DZEF::Type::STRING: {
                    std::string v; std::cout << "String value: "; std::cin >> v;
                    enc.addNamedString(name, v);
                    break;
                }
                default:
                    std::cout << "Invalid type!\n";
                    break;
            }
        }

        std::cout << "Add another entry? (1=yes, 0=no): ";
        std::cin >> addMore;
    }

    outFile.close();

    std::ifstream inFile("test.bin", std::ios::binary);
    DZEF::Decoder decoder(inFile);
    std::cout << "\nDecoded Output:\n";
    decoder.decode();
    inFile.close();

    return 0;
}

