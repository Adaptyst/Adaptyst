#ifndef ADAPTYST_OUTPUT_HPP_
#define ADAPTYST_OUTPUT_HPP_

#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <fstream>

namespace adaptyst {
  namespace fs = std::filesystem;

  class ObjectWithMetadata {
  protected:
    nlohmann::json metadata;

  public:
    ObjectWithMetadata() {
      this->metadata = nlohmann::json::object();
    }

    template<class T>
    void set_metadata(std::string key, T value,
                      bool save = true) {
      if (this->metadata[key] != value) {
        this->metadata[key] = value;

        if (save) {
          this->save_metadata();
        }
      }
    }

    template<class T>
    T get_metadata(std::string key, T default_value) {
      return this->metadata.value(key, default_value);
    }

    template<class T>
    T get_metadata(std::string key) {
      return this->metadata[key].get<T>();
    }

    virtual void save_metadata() = 0;
  };

  class Path : public ObjectWithMetadata {
    friend class File;

  private:
    fs::path path;

    void setup(fs::path path) {
      this->path = fs::absolute(path);
      try {
        fs::create_directories(this->path);
      } catch (std::exception &e) {
        throw std::runtime_error("Could not create directory " +
                                 this->path.string() + ": " +
                                 std::string(e.what()));
      }

      fs::path metadata_path = this->path / "dirmeta.json";

      if (fs::exists(metadata_path)) {
        std::ifstream metadata_stream(metadata_path);

        if (!metadata_stream) {
          throw std::runtime_error("Could not open " +
                                 (this->path / "dirmeta.json").string() +
                                 " for reading!");
        }

        std::string json_str((std::istreambuf_iterator<char>(metadata_stream)),
                             std::istreambuf_iterator<char>());
        this->metadata = nlohmann::json::parse(json_str);
      }
    }

  public:
    Path(fs::path path) {
      this->setup(path);
    }

    const char *get_path_name() {
      return this->path.c_str();
    }

    Path operator/(std::string second) {
      return Path(this->path / second);
    }

    Path operator/(const char *second) {
      return Path(this->path / second);
    }

    void save_metadata() {
      std::ofstream metadata_stream(this->path / "dirmeta.json");

      if (!metadata_stream) {
        throw std::runtime_error("Could not open " +
                                 (this->path / "dirmeta.json").string() +
                                 " for writing!");
      }

      metadata_stream << this->metadata.dump() << std::endl;
    }
  };

  class File : public ObjectWithMetadata {
  protected:
    Path &path;
    std::string name;
    std::ifstream istream;
    std::ofstream ostream;

  public:
    File(Path &path, std::string name,
         std::string extension = "",
         bool truncate = true) : path(path) {
      this->name = name;

      fs::path new_path = path.path / (name + extension);

      if (fs::exists(new_path)) {
        if (fs::is_directory(new_path)) {
          throw std::runtime_error(new_path.string() + " is "
                                   "a directory");
        } else {
          this->istream = std::ifstream(new_path);
        }
      }

      this->ostream = std::ofstream(new_path, std::ios_base::out |
                                    (truncate ?
                                     std::ios_base::trunc : std::ios_base::app));

      if (!this->ostream) {
        throw std::runtime_error("Could not open " +
                                 new_path.string() +
                                 " for writing!");
      }

      fs::path metadata_path = path.path / ("meta_" + this->name + ".json");

      if (fs::exists(metadata_path)) {
        std::ifstream metadata_stream(metadata_path);

        if (!metadata_stream) {
          throw std::runtime_error("Could not open " +
                                 (path.path / ("meta_" + this->name + ".json")).string() +
                                 " for reading!");
        }

        std::string json_str((std::istreambuf_iterator<char>(metadata_stream)),
                             std::istreambuf_iterator<char>());
        this->metadata = nlohmann::json::parse(json_str);
      }
    }

    File &operator=(File &&source) {
      this->path = source.path;
      this->name = source.name;
      this->istream = std::move(source.istream);
      this->ostream = std::move(source.ostream);
      this->metadata.swap(source.metadata);
      return *this;
    }

    std::ifstream &get_istream() {
      return this->istream;
    }

    std::ofstream &get_ostream() {
      return this->ostream;
    }

    void save_metadata() {
      fs::path metadata_path = path.path / ("meta_" + this->name + ".json");

      std::ofstream metadata_stream(metadata_path);

      if (!metadata_stream) {
        throw std::runtime_error("Could not open " + metadata_path.string() +
                                 " for writing!");
      }

      metadata_stream << this->metadata.dump() << std::endl;
    }
  };

  template<typename T>
  concept is_pair = requires {
    typename T::first_type;
    typename T::second_type;
  } && std::is_same_v<T, std::pair<typename T::first_type,
                                   typename T::second_type> >;

  template<class T>
  class Array : public File {
  private:
    std::vector<T> vec;

  public:
    Array(Path &path, std::string name) : File(path, name, ".dat", false) {
      while (this->istream && !this->istream.eof()) {
        T val;

        if constexpr (is_pair<T>) {
          this->istream >> val.first;
          this->istream >> val.second;
        } else {
          this->istream >> val;
        }

        if (this->istream) {
          vec.push_back(val);
        }
      }
    }

    T operator[](int index) {
      return this->vec[index];
    }

    int size() {
      return this->vec.size();
    }

    void push_back(T val) {
      this->vec.push_back(val);

      if constexpr (is_pair<T>) {
        this->ostream << val.first << " " << val.second << std::endl;
      } else {
        this->ostream << val << std::endl;
      }
    }
  };
}

#endif
