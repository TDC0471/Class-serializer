#pragma once

#include <istream>
#include <ostream>
#include <fstream>
#include <iostream>
#include <list>

//simple hash function to convert a string to a size_t
size_t hash(const char* str)
{
	size_t hash = 5381;
	int c;
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}

struct Footer
{
	static const unsigned char footersig[4];
	const unsigned char footer[4]{ 0xEF, 0xBE, 0xAD, 0xDE };

	Footer() {}

	Footer(void* address)
	{
		std::memcpy(const_cast<unsigned char*>(footer), address, 4);
	}

	static inline size_t size() { return sizeof(unsigned char) * 4; }

	// Boolean function to check if the footer is valid
	bool isValid() { return std::memcmp(footer, footersig, 4) == 0; }

	static void write(void* address) { std::memcpy(address, footersig, 4); }

	// Write the footer to a file
	static void write(std::ostream& os) { os.write((char*)footersig, 4); }

	// Read the footer from a file
	void read(std::istream& is) { is.read((char*)footer, 4); }
};

// Initializing the static member outside the class definition
const unsigned char Footer::footersig[4] = { 0xEF, 0xBE, 0xAD, 0xDE };

class Header
{

	size_t hashedName;
	size_t sizeOfModule;

public:
	size_t getHashedName() { return hashedName; }
	size_t getSizeOfModule() { return sizeOfModule; }
	size_t setSizeOfModule(size_t size) { sizeOfModule = size; }

	Header(size_t hashedName, size_t sizeOfModule) : hashedName(hashedName), sizeOfModule(sizeOfModule) {}
	Header(char* address) : hashedName(*reinterpret_cast<size_t*>(address)), sizeOfModule(*reinterpret_cast<size_t*>(address + sizeof(size_t))) { }

	static inline size_t size() { return sizeof(size_t) * 2; }

	void write(void* address)
	{
		std::memcpy(address, &hashedName, sizeof(size_t));
		std::memcpy(static_cast<char*>(address) + sizeof(size_t), &sizeOfModule, sizeof(size_t));
	}

	void write(std::ostream& os)
	{
		os.write((char*)&hashedName, sizeof(size_t));
		os.write((char*)&sizeOfModule, sizeof(size_t));
	}

	void read(std::istream& is)
	{
		is.read((char*)&hashedName, sizeof(size_t));
		is.read((char*)&sizeOfModule, sizeof(size_t));
	}
};

struct UnknownElement
{
	const size_t hashedName;
	const unsigned char dataType;

public:
	UnknownElement(int hashedName, unsigned char dataType) : hashedName(hashedName), dataType(dataType) {}

	UnknownElement(char* address) :
		hashedName(*reinterpret_cast<size_t*>(address)),
		dataType(*reinterpret_cast<unsigned char*>(address + sizeof(size_t)))
	{

	}

	static inline size_t size() { return sizeof(size_t) + sizeof(unsigned char); }

	void write(std::ostream& os)
	{
		os.write((char*)&hashedName, sizeof(size_t));
		os.write((char*)&dataType, sizeof(unsigned char));
	}

	void write(void* address)
	{
		std::memcpy(address, &hashedName, sizeof(size_t));
		std::memcpy(static_cast<char*>(address) + sizeof(size_t), &dataType, sizeof(unsigned char));
	}

	void read(std::istream& is)
	{
		is.read((char*)&hashedName, sizeof(size_t));
		is.read((char*)&dataType, sizeof(unsigned char));
	}


};

template<typename T>
struct ElementData
{
	size_t hashedName;
	T data;

	ElementData(const size_t& hashedName, const T& data) : hashedName(hashedName), data(data) {}
	ElementData(size_t&& hashedName, T&& data) : hashedName(hashedName), data(data) {}
};

template<typename T>
class Element : UnknownElement
{
	T data;

public:
	Element(size_t hashedName, const T& data) : UnknownElement(hashedName, typeToId<T>()), data(data) {}
	Element(const UnknownElement& UE, char* UEAddress) : UnknownElement(UE.hashedName, UE.dataType), data(*reinterpret_cast<T*>(UEAddress + UnknownElement::size())) {}
	Element(const ElementData<T>& ED) : UnknownElement(ED.hashedName, typeToId<T>()), data(ED.data) {}

	static inline size_t size() { return UnknownElement::size() + sizeof(T); }

	explicit operator T () { return data; }

	void write(std::ostream& os)
	{
		UnknownElement::write(os);
		os.write((char*)&data, sizeof(T));
	}

	void write(void* address)
	{
		UnknownElement::write(address);
		std::memcpy(static_cast<char*>(address) + UnknownElement::size(), &data, sizeof(T));
	}

	void read(std::istream& is)
	{
		UnknownElement::read(is);
		is.read((char*)&data, sizeof(T));
	}
};

template<typename T>
unsigned char typeToId()
{
	if (typeid(T) == typeid(int))
		return 0x01;
	else if (typeid(T) == typeid(float))
		return 0x02;
}

size_t idToTSize(unsigned char id)
{
	switch (id)
	{
	case 0x01:
		return sizeof(int);
	case 0x02:
		return sizeof(float);
	}
}


class Config
{
	typedef void /*const*/* module_t;
	typedef void* rammodule_t;
	char* buffer;
	size_t size;
	const char* filename;

public:
	Config(const char* filename) : filename(filename)
	{
		// open the file
		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		// check if the file is open
		if (!file.is_open())
			throw std::exception("Error opening file!");

		if (file.fail())
			throw std::exception("Error reading file!");

		// get the size of the file
		size = file.tellg();

		// allocate buffer
		buffer = new char[size];

		// read file into buffer
		file.seekg(0, std::ios::beg);
		file.read(buffer, size);

		file.close();
	}

	void save()
	{
		std::ofstream file(filename, std::ios::out);

		if (!file.is_open())
			throw std::exception("Error opening file!");

		file.write(buffer, size);

		file.close();
	}

	~Config()
	{
		//save buffer to file and delete buffer
		//std::ofstream file(filename, std::ios::binary | std::ios::ate);
		delete[] buffer;
	}


	void cleanModules(const std::list<size_t>& modules) //delete all modules except the ones in the list
	{
		char* pos = buffer;

		const char* end = buffer + size;
		while (pos < end)
		{
			Header header(pos);
			auto it = std::find(modules.begin(), modules.end(), header.getHashedName());
			//if the module is not in the list, delete it
			if (it == modules.end())
				deleteModule(reinterpret_cast<void*>(pos));
			else
			{
				//get the footer
				Footer footer{ pos + header.getSizeOfModule() - Footer::size() };

				if (!footer.isValid())
					throw std::exception("Expected footed not found in \"cleanModules\" likely corrupted save");

				pos += header.getSizeOfModule();
			}
		}
		buffer = static_cast<char*>(realloc(buffer, size));
	}


	template<typename... Ts>
	void getModuleData(size_t hashedName, const ElementData<Ts&>&... variables)
	{
		if (module_t module = getModule(hashedName); module)
		{
			getElementsValues(module, variables...);
			return;
		}
		
		//if module doesn't exist, load default values into variables
		loadDefaultValues(variables...);
	}

	template<typename... Ts>
	void saveModule(size_t hashedName, const ElementData<Ts>&... pairs) 
	{
		if (module_t module = getModule(hashedName); module) 
		{
			setElementsValues(module, pairs...);
			return;
		}

		createModule(hashedName, pairs...);
	}


private:

	template<typename T>
	void loadDefaultValues(const ElementData<T&>& variable)
	{
		variable.data = T{};
	}

	// Overload the function for multiple variables
	template<typename T, typename... Ts>
	void loadDefaultValues(const ElementData<T&>& variable, const ElementData<Ts&>&... variables)
	{
		variable.data = T{};
		loadDefaultValues(variables...);
	}

	module_t getModule(const size_t& hashedName)
	{
		char* pos = buffer;

		const char* end = buffer + size;
		while (pos < end)
		{
			Header header(pos);
			if (header.getHashedName() == hashedName)
				return pos;

			//get the footer
			Footer footer{ pos + header.getSizeOfModule() - Footer::size() };

			if (!footer.isValid())
				throw std::exception("Expected footed not found in \"getModule\" likely corrupted save");

			pos += header.getSizeOfModule();

		}
		return nullptr;
	}

	template <typename T, typename... Ts>
	void getElementsValues(module_t module, const ElementData<T&>& var, const ElementData<Ts&>&... varables)
	{
		var.data = getElementValue<T>(module, var.hashedName);
		getElementsValues(module, varables...);
	}
	void getElementsValues(module_t module) {} //base case

	template<typename T, typename... Ts>
	void setElementsValues(module_t module, const ElementData<T>& element, const ElementData<Ts>&... elements) 
	{
		setElementValue(module, element.hashedName, element.data);
		setElementsValues(module, elements...);
	}
	void setElementsValues(module_t module) {} //base case


	template<typename T>
	T getElementValue(module_t module, const size_t& hashedName)
	{
		char* pos = static_cast<char*>(getElementAddress(module, hashedName));

		if (!pos) return T();

		return *reinterpret_cast<T*>(pos += UnknownElement::size());
	}

	template<typename T>
	void setElementValue(module_t module, const size_t& hashedName, const T& data)
	{
		char* pos = static_cast<char*>(getElementAddress(module, hashedName));

		if (!pos)
		{
			addElements<T>(module, { hashedName, data });
		}

		*reinterpret_cast<T*>(pos += UnknownElement::size()) = data;
	}

	template<typename... Ts>
	module_t createModule(size_t hashedName, const ElementData<Ts>&... pairs)
	{
		// Calculate the size of the module
		size_t size = Header::size() + Footer::size() + (UnknownElement::size() * sizeof...(pairs)) + (sizeof(Ts) + ...);
		rammodule_t modulealloc = malloc(size);
		Header header(hashedName, size);
		header.write(modulealloc);

		addElementInternal(static_cast<char*>(modulealloc) + Header::size(), pairs...);

		Footer::write(static_cast<char*>(modulealloc) + size - Footer::size());

		module_t tmp = addModule(modulealloc);
		free(modulealloc);
		return tmp;
	}

	template<typename... Ts>
	void addElements(module_t module, const ElementData<Ts>... pairs)
	{
		// Calculate the size of the module
		Header tmpmod(static_cast<char*>(module));
		size_t size = tmpmod.getSizeOfModule() + (UnknownElement::size() * sizeof...(pairs)) + (sizeof(Ts) + ...);

		rammodule_t newModule = copyModule(module, size);
		deleteModule(module);
		buffer = static_cast<char*>(realloc(buffer, size));

		addElementInternal(static_cast<char*>(newModule) + tmpmod.getSizeOfModule(), pairs...);

		Footer::write(static_cast<char*>(newModule) + size - Footer::size());
		addModule(newModule);
		free(newModule);
	}

	void addElementInternal(char* address) {}

	template<typename T, typename... Ts>
	void addElementInternal(char* address, const ElementData<T>& pair, const ElementData<Ts>&... pairs)
	{
		Element<T> element(pair);
		element.write(address);
		addElementInternal(address += element.size(), pairs...);
	}

	template<typename... Ts>
	void addElementInternal(char* address, const ElementData<Ts>&... pairs)
	{
		addElementInternal(address, pairs...);
	}

	rammodule_t copyModule(module_t module) //Copies module into a new location in memory and deletes the old one
	{
		Header header(static_cast<char*>(module));
		void* newModule = malloc(header.getSizeOfModule());
		memcpy(newModule, module, header.getSizeOfModule());

		return newModule;
	}

	rammodule_t copyModule(module_t module, size_t size) //Copies module into a new location in memory and deletes the old one
	{
		Header header(static_cast<char*>(module));
		void* newModule = malloc(size);
		memcpy(newModule, module, header.getSizeOfModule());
		return newModule;
	}

	module_t addModule(rammodule_t module) // adds a module to the end of the buffer
	{
		Header header(static_cast<char*>(module));
		size += header.getSizeOfModule();
		buffer = static_cast<char*>(realloc(buffer, size));
		memcpy(buffer + size - header.getSizeOfModule(), module, header.getSizeOfModule());

		return buffer + size - header.getSizeOfModule();
	}

	void deleteModule(module_t module)
	{
		Header header(static_cast<char*>(module));
		memmove(module, static_cast<char*>(module) + header.getSizeOfModule(), size - reinterpret_cast<uintptr_t>(module) - header.getSizeOfModule());
		size -= header.getSizeOfModule();
	}

	// Precondidtion: module is a valid module (queried from getModule)
	void* getElementAddress(module_t module, const size_t& hashedName) //ERROR!
	{
		std::cout << "Hashedname: " << hashedName << std::endl;
		Header header(static_cast<char*>(module));
		char* pos = static_cast<char*>(module) + Header::size();


		while (pos - module < header.getSizeOfModule() - Footer::size())
		{

			UnknownElement UElem(pos);
			std::cout << UElem.hashedName << " " << (UElem.hashedName == hashedName) << std::endl;
			if (UElem.hashedName == hashedName)
				return pos;

			pos += (UnknownElement::size() + idToTSize(UElem.dataType));

		}
		if (Footer(pos).isValid())
			return nullptr;

		throw std::exception("Footer not found in \"getElementAddress\" likely corrupted save");
	}

};