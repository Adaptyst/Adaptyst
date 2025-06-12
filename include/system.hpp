#ifndef SYSTEM_HPP_
#define SYSTEM_HPP_

#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <future>
#include "adaptyst/output.hpp"
#include "adaptyst/process.hpp"

#define ADAPTYST_INTERNAL
#include "adaptyst/hw.h"

namespace adaptyst {
  namespace fs = std::filesystem;

  class Identifiable {
  private:
    std::string id;

  protected:
    Identifiable(std::string id);
    inline void throw_error(std::string msg) {
      throw std::runtime_error(id + ": " + msg);
    }

  public:
    std::string get_id();
    const char *get_id_c_str();
    virtual std::vector<std::string> get_log_types() = 0;
    virtual std::string get_type() = 0;
  };

  class Entity;
  class System;

  class Node : public Identifiable {
  public:
    static std::unordered_map<amod_t, Node *> all_nodes;
    static amod_t next_node_id;
    static std::unordered_map<amod_t, std::string> internal_error_msgs;
    static std::unordered_map<amod_t, int> internal_error_codes;

    struct OptionMetadata {
      std::string help;
      option_type type;
      option_type array_type;
      void *default_value;
      void *default_array_value;
      unsigned int default_array_value_size;
    };

    Node(std::string id, std::string backend,
         std::unordered_map<std::string, std::string> &options,
         std::unordered_map<std::string, std::vector<std::string>>
             &array_options,
         std::shared_ptr<Entity> &entity,
         fs::path library_path);
    ~Node();
    std::shared_ptr<Entity> &get_entity();
    bool init();
    void process(std::string &sdfg);
    bool wait();
    void close();
    void set_will_profile(bool will_profile);
    bool get_will_profile();
    void set_error(std::string error);
    void set_context(void *context);
    void *get_context();
    std::unordered_map<std::string, option> &get_options();
    std::unique_ptr<Path> &get_node_dir();
    std::unordered_set<std::string> &get_tags();
    std::unordered_map<std::string, OptionMetadata> &get_all_options();
    void set_node_dir(fs::path &node_dir);
    void profile_notify();
    int profile_wait();
    void add_in_tags(std::unordered_set<std::string> &tags);
    void add_out_tags(std::unordered_set<std::string> &tags);
    bool has_in_tag(std::string tag);
    bool has_out_tag(std::string tag);
    std::vector<std::string> get_log_types();
    std::string get_type();

  private:
    amod_t module_id;
    std::string backend;
    std::shared_ptr<Entity> entity;
    std::unordered_map<std::string, option> options;
    std::unordered_map<std::string, OptionMetadata> option_metadata;
    std::unique_ptr<Path> node_dir;
    bool will_profile;
    std::string error;
    void *context;
    std::unordered_set<std::string> tags;
    std::unordered_set<std::string> in_tags;
    std::unordered_set<std::string> out_tags;
    void *backend_handle;
    std::future<bool> process_future;
    std::vector<std::string> log_types;
    std::vector<void *> malloced;
  };

  class NodeConnection : public Identifiable {
  private:
    std::shared_ptr<Node> departure_node;
    std::shared_ptr<Node> arrival_node;

  public:
    NodeConnection(std::string id,
                   std::shared_ptr<Node> &departure_node,
                   std::shared_ptr<Node> &arrival_node);
    std::shared_ptr<Node> &get_departure_node();
    std::shared_ptr<Node> &get_arrival_node();
    std::vector<std::string> get_log_types();
    std::string get_type();
  };

  class Entity : public Identifiable {
  public:
    enum AccessMode {
      IN_PLACE,
      REMOTE,
      CUSTOM,
      CUSTOM_REMOTE
    };

    Entity(std::string id, AccessMode access_mode,
           unsigned int processing_threads,
           fs::path local_config_path,
           fs::path tmp_dir);
    void add_node(std::shared_ptr<Node> &node);
    void add_connection(std::string id,
                        std::string departure_node,
                        std::string arrival_node);
    std::shared_ptr<Node> &get_node(std::string id);
    void set_directing_node(std::string node);
    std::string get_directing_node();
    profile_info &get_profile_info();
    void set_profile_info(profile_info &info);
    void init();
    void process(bool save_src_code_paths);
    void close();
    void set_entity_dir(fs::path &entity_dir);
    void profile_notify();
    int profile_wait();
    const char *get_cpu_mask();
    fs::path get_tmp_dir();
    fs::path get_local_config_dir();
    std::vector<std::shared_ptr<Identifiable> > get_all_nodes();
    std::vector<std::string> get_log_types();
    std::string get_type();
    void set_sdfg(std::string sdfg);
    void add_src_code_path(fs::path path);
    std::unordered_set<fs::path> &get_src_code_paths();

  private:
    AccessMode access_mode;
    std::unordered_map<std::string, std::shared_ptr<Node> > nodes;
    std::unordered_map<std::string, std::shared_ptr<NodeConnection> > connections;
    std::string directing_node;
    profile_info profiling_info;
    std::unique_ptr<Path> entity_dir;
    std::unordered_set<hid_t> hdf5_groups;
    unsigned int processing_threads;
    fs::path local_config_path;
    fs::path tmp_dir;
    std::string cpu_mask;
    std::string sdfg;
    bool will_profile;
    std::unique_ptr<Process> profiled_process;
    std::unordered_set<fs::path> src_code_paths;
  };

  class System {
  private:
    std::unordered_map<std::string, std::shared_ptr<Entity> > entities;
    std::unordered_map<std::string,
                       std::shared_ptr<NodeConnection> > connections;
    std::unique_ptr<Path> root_dir;
    std::variant<fs::path, int> codes_dst;
    bool custom_src_code_paths_save;

    void init(fs::path def_file, fs::path root_dir,
              fs::path library_path, fs::path local_config_path,
              fs::path tmp_dir);
  public:
    System(fs::path def_file, fs::path root_dir,
           fs::path library_path, fs::path local_config_path,
           fs::path tmp_dir);
    System(fs::path def_file, fs::path root_dir,
           fs::path library_path, fs::path local_config_path,
           fs::path tmp_dir, std::variant<fs::path, int> codes_dst);
    ~System();
    std::vector<std::shared_ptr<Identifiable> > get_all_identifiables();
    void set_sdfg(std::string sdfg);
    void process();
    bool with_custom_src_code_paths();
  };
};

#endif
