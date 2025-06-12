#include "system.hpp"
#include "print.hpp"
#include "archive.hpp"
#include "adaptyst/output.hpp"
#include <ryml.hpp>
#include <fstream>
#include <dlfcn.h>
#include <pybind11/embed.h>

// The code segment below is for the C hardware module API.
inline adaptyst::Node *get(amod_t id) {
  if (adaptyst::Node::all_nodes.find(id) == adaptyst::Node::all_nodes.end()) {
    return nullptr;
  }

  return adaptyst::Node::all_nodes[id];
}

inline void set_error(amod_t id, int code) {
  std::string msg;

  switch (code) {
  case ADAPTYST_ERR_MODULE_NOT_FOUND:
    msg = "Module not found";
    break;

  case ADAPTYST_ERR_OUT_OF_MEMORY:
    msg = "Out of memory";
    break;
  }

  adaptyst::Node::internal_error_msgs[id] = msg;
  adaptyst::Node::internal_error_codes[id] = code;
}

inline void set_error(amod_t id, std::string msg, int code) {
  adaptyst::Node::internal_error_msgs[id] = msg;
  adaptyst::Node::internal_error_codes[id] = code;
}

extern "C" {
  bool adaptyst_new_context(amod_t id, unsigned int size) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    void *context = malloc(size);

    if (!context) {
      set_error(id, ADAPTYST_ERR_OUT_OF_MEMORY);
      return false;
    }

    node->set_context(context);
    return true;
  }

  void *adaptyst_get_context(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    return node->get_context();
  }

  option *adaptyst_get_option(amod_t id, const char *key) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    auto &options = node->get_options();
    std::string key_str(key);

    if (options.find(key_str) == options.end()) {
      return NULL;
    }

    return &options[key_str];
  }

  bool adaptyst_set_error(amod_t id, const char *msg) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    node->set_error(std::string(msg));
    return true;
  }

  const char *adaptyst_get_log_dir(amod_t id) {
    if (!adaptyst::Terminal::instance) {
      set_error(id, ADAPTYST_ERR_TERMINAL_NOT_INITIALISED);
      return NULL;
    }

    return adaptyst::Terminal::instance->get_log_dir();
  }

  const char *adaptyst_get_node_id(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    return node->get_id_c_str();
  }

  bool adaptyst_log(amod_t id, const char *msg, const char *type) {
    if (!adaptyst::Terminal::instance) {
      set_error(id, ADAPTYST_ERR_TERMINAL_NOT_INITIALISED);
      return false;
    }

    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    try {
      adaptyst::Terminal::instance->log(std::string(msg), node,
                                        std::string(type));
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return false;
    }

    return true;
  }

  bool adaptyst_print(amod_t id, const char *msg, bool sub, bool error,
                      const char *type) {
    if (!adaptyst::Terminal::instance) {
      return false;
    }

    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    try {
      adaptyst::Terminal::instance->print(std::string(msg), sub, error, node,
                                          std::string(type));
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return false;
    }

    return true;
  }

  const char *adaptyst_get_node_dir(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    try {
      return node->get_node_dir()->get_path_name();
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return NULL;
    }
  }

  profile_info *adaptyst_get_profile_info(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    auto &profile_info = node->get_entity()->get_profile_info();
    return &profile_info;
  }

  bool adaptyst_set_profile_info(amod_t id, profile_info *info) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    node->get_entity()->set_profile_info(*info);
    return true;
  }

  bool adaptyst_is_directing_node(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    return node->get_entity()->get_directing_node() == node->get_id();
  }

  bool adaptyst_profile_notify(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    try {
      node->get_entity()->profile_notify();
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return false;
    }

    return true;
  }

  int adaptyst_profile_wait(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return -1;
    }

    try {
      return node->get_entity()->profile_wait();
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return -1;
    }
  }

  bool adaptyst_process_src_paths(amod_t id, const char **paths, int n) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    for (int i = 0; i < n; i++) {
      node->get_entity()->add_src_code_path(paths[i]);
    }

    return true;
  }

  const char *adaptyst_get_cpu_mask(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    try {
      return node->get_entity()->get_cpu_mask();
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return NULL;
    }
  }

  const char *adaptyst_get_tmp_dir(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    try {
      return node->get_entity()->get_tmp_dir().c_str();
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return NULL;
    }
  }

  const char *adaptyst_get_local_config_dir(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    try {
      return node->get_entity()->get_local_config_dir().c_str();
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return NULL;
    }
  }

  bool adaptyst_set_will_profile(amod_t id, bool will_profile) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    node->set_will_profile(will_profile);
    return true;
  }

  bool adaptyst_has_in_tag(amod_t id, const char *tag) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    try {
      return node->has_in_tag(std::string(tag));
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return false;
    }
  }

  bool adaptyst_has_out_tag(amod_t id, const char *tag) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return false;
    }

    try {
      return node->has_out_tag(std::string(tag));
    } catch (std::exception &e) {
      set_error(id, std::string(e.what()), ADAPTYST_ERR_EXCEPTION);
      return false;
    }
  }

  const char ***adaptyst_get_in_tags(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    // TODO
  }

  const char ***adaptyst_get_out_tags(amod_t id) {
    auto node = get(id);

    if (!node) {
      set_error(id, ADAPTYST_ERR_MODULE_NOT_FOUND);
      return NULL;
    }

    // TODO
  }

  const char *adaptyst_get_internal_error_msg(amod_t id) {
    if (adaptyst::Node::internal_error_msgs.find(id) ==
        adaptyst::Node::internal_error_msgs.end()) {
      return NULL;
    }

    return adaptyst::Node::internal_error_msgs[id].c_str();
  }

  int adaptyst_get_internal_error_code(amod_t id) {
    if (adaptyst::Node::internal_error_codes.find(id) ==
        adaptyst::Node::internal_error_codes.end()) {
      return 0;
    }

    return adaptyst::Node::internal_error_codes[id];
  }
}
// The C hardware module API code segment ends here.

namespace adaptyst {
  namespace py = pybind11;

  std::unordered_map<amod_t, Node *> Node::all_nodes;
  amod_t Node::next_node_id = 1;
  std::unordered_map<amod_t, std::string> Node::internal_error_msgs;
  std::unordered_map<amod_t, int> Node::internal_error_codes;

  Identifiable::Identifiable(std::string id) {
    this->id = id;
  }

  std::string Identifiable::get_id() {
    return this->id;
  }

  const char *Identifiable::get_id_c_str() {
    return this->id.c_str();
  }

  Node::Node(std::string id,
             std::string backend,
             std::unordered_map<std::string, std::string> &options,
             std::unordered_map<std::string, std::vector<std::string> > &array_options,
             std::shared_ptr<Entity> &entity,
             fs::path library_path) : Identifiable(id) {
    this->entity = entity;
    this->backend = backend;

    fs::path lib_path = library_path / ("lib" + backend + ".so");

    this->backend_handle = dlopen(lib_path.c_str(), RTLD_LAZY);

    if (!this->backend_handle) {
      this->throw_error("Could not load backend \"" + backend +
                        "\"! " + std::string(dlerror()));
    }

    const char **tags = (const char **)dlsym(this->backend_handle, "tags");

    if (!tags) {
      this->throw_error("Backend \"" + backend + "\" doesn't define its tags!");
    }

    for (int i = 0; tags[i]; i++) {
      this->tags.insert(std::string(tags[i]));
    }

    const char **backend_options = (const char **)dlsym(this->backend_handle, "options");

    if (!backend_options) {
      this->throw_error("Backend \"" + backend + "\" doesn't define "
                        "what options are available!");
    }

    for (int i = 0; backend_options[i]; i++) {
      std::string name(backend_options[i]);
      OptionMetadata metadata;

      const char **help = (const char **)dlsym(this->backend_handle,
                                             (name + "_help").c_str());

      if (!help) {
        this->throw_error("Backend \"" + backend + "\" doesn't define any "
                          "help message for option \"" + name + "\"!");
      }

      metadata.help = std::string(*help);

      option_type *type = (option_type *)dlsym(this->backend_handle,
                                               (name + "_type").c_str());

      if (type) {
        metadata.type = *type;
      } else {
        metadata.type = NONE;
      }

      option_type *array_type = (option_type *)dlsym(this->backend_handle,
                                                     (name + "_array_type").c_str());

      if (array_type) {
        metadata.array_type = *array_type;
      } else {
        metadata.array_type = NONE;
      }

      if (!type && !array_type) {
        this->throw_error("Backend \"" + backend + "\" doesn't define any "
                          "type for option \"" + name + "\"!");
      }

      metadata.default_value = dlsym(this->backend_handle,
                                     (name + "_default").c_str());
      metadata.default_array_value = dlsym(this->backend_handle,
                                           (name + "_array_default").c_str());

      unsigned int *default_array_value_size =
        (unsigned int *)dlsym(this->backend_handle,
                              (name + "_array_default_size").c_str());

      if (default_array_value_size) {
        metadata.default_array_value_size = *default_array_value_size;
      } else {
        metadata.default_array_value_size = 0;
      }

      this->option_metadata[name] = metadata;

      if (options.find(name) == options.end() &&
          array_options.find(name) == array_options.end()) {
        if (metadata.type != NONE && metadata.default_value) {
          this->options[name] = option(metadata.type, metadata.default_value, 0);
        } else if (metadata.array_type != NONE &&
                   metadata.default_array_value) {
          this->options[name] = option(metadata.array_type,
                                       metadata.default_array_value,
                                       metadata.default_array_value_size);
        } else {
          this->throw_error("Backend \"" + backend +
                            "\" requires option "
                            "\"" +
                            name + "\" to be set!");
        }
      } else if (options.find(name) != options.end()) {
        void *allocated = NULL;

        switch (metadata.type) {
        case INT:
          allocated = malloc(sizeof(int));
          break;

        case STRING:
          allocated = malloc(sizeof(char *));
          break;

        case UNSIGNED_INT:
          allocated = malloc(sizeof(unsigned int));
          break;

        case BOOL:
          allocated = malloc(sizeof(bool));
          break;

        default:
          throw std::runtime_error("Unsupported option type for \"" + name + "\"");
        }

        if (!allocated) {
          throw std::runtime_error("Could not allocate memory for \"" + name + "\"");
        }

        malloced.push_back(allocated);
        void *str = NULL;

        try {
          switch (metadata.type) {
          case INT:
            *((int *)allocated) = std::stoi(options[name]);
            break;

          case STRING:
            str = calloc(options[name].length() + 1,
                         sizeof(char));

            if (!str) {
              throw std::runtime_error("Could not allocate memory for \"" + name + "\" "
                                       "(stage 2)");
            }

            malloced.push_back(str);

            strncpy((char *)str, options[name].c_str(),
                    options[name].length());
            *((char **)allocated) = (char *)str;
            break;

          case UNSIGNED_INT:
            *((unsigned int *)allocated) = std::stoul(options[name]);
            break;

          case BOOL:
            std::istringstream(options[name]) >> std::boolalpha >>
                *((bool *)allocated);
            break;

          default:
            // This should have been caught in the previous "switch"
            break;
          }
        } catch (std::invalid_argument) {
          throw std::runtime_error("Could not parse value of \"" + name + "\"");
        } catch (std::out_of_range) {
          throw std::runtime_error("Could not parse value of \"" + name + "\"");
        }

        this->options[name] = option(metadata.type, allocated, 0);
      } else {
        void *allocated = NULL;

        switch (metadata.array_type) {
        case INT:
          allocated = malloc(array_options[name].size() * sizeof(int));
          break;

        case STRING:
          allocated = malloc(array_options[name].size() * sizeof(char *));
          break;

        case UNSIGNED_INT:
          allocated = malloc(array_options[name].size() * sizeof(unsigned int));
          break;

        case BOOL:
          allocated = malloc(array_options[name].size() * sizeof(bool));
          break;

        default:
          throw std::runtime_error("Unsupported option array type for \"" + name + "\"");
        }

        if (!allocated) {
          throw std::runtime_error("Could not allocate memory for \"" + name + "\" (array)");
        }

        malloced.push_back(allocated);

        int i = -1;

        try {
          switch (metadata.array_type) {
          case INT:
            for (i = 0; i < array_options[name].size(); i++) {
              ((int *)allocated)[i] = std::stoi(array_options[name][i]);
            }
            break;

          case STRING:
            for (i = 0; i < array_options[name].size(); i++) {
              void *element = calloc(array_options[name][i].length() + 1,
                                     sizeof(char));

              if (!element) {
                throw std::runtime_error("Could not allocate memory for element of index " +
                                         std::to_string(i) + " of \"" + name + "\"");
              }

              malloced.push_back(element);

              strncpy((char *)element, array_options[name][i].c_str(),
                      array_options[name][i].length());

              ((char **)allocated)[i] = (char *) element;
            }
            break;

          case UNSIGNED_INT:
            for (i = 0; i < array_options[name].size(); i++) {
              ((unsigned int *)allocated)[i] = std::stoul(array_options[name][i]);
            }
            break;

          case BOOL:
            for (i = 0; i < array_options[name].size(); i++) {
              std::istringstream(array_options[name][i]) >> std::boolalpha >>
                ((bool *)allocated)[i];
            }
            break;

          default:
            // This should have been caught in the previous "switch"
            break;
          }
        } catch (std::invalid_argument) {
          throw std::runtime_error("Could not parse value of element of index " +
                                   std::to_string(i) + " of \"" + name + "\"");
        } catch (std::out_of_range) {
          throw std::runtime_error("Could not parse value of element of index " +
                                   std::to_string(i) + " of \"" + name + "\"");
        }

        this->options[name] =
          option(metadata.array_type, &allocated, array_options[name].size());
      }
    }

    const char **log_types =
      (const char **)dlsym(this->backend_handle, "log_types");

    if (log_types) {
      for (int i = 0; log_types[i]; i++) {
        this->log_types.push_back(std::string(log_types[i]));
      }
    } else {
      this->log_types = {"General"};
    }

    this->module_id = Node::next_node_id++;
    Node::all_nodes[this->module_id] = this;
  }

  Node::~Node() {
    for (auto &allocated : this->malloced) {
      free(allocated);
    }

    dlclose(this->backend_handle);
  }

  std::shared_ptr<Entity> &Node::get_entity() {
    return this->entity;
  }

  bool Node::init() {
    bool (*init_func)(amod_t) =
      (bool (*)(amod_t))dlsym(this->backend_handle, "_adaptyst_module_init");

    if (!init_func) {
      this->throw_error("Backend \"" + this->backend + "\" doesn't define _adaptyst_module_init()! "
                        "Has it been compiled correctly?");
    }

    bool result = init_func(this->module_id);

    if (!result && !this->error.empty()) {
      this->throw_error(this->error);
    }

    return result;
  }


  void Node::process(std::string &sdfg) {
    bool (*process_func)(const char *) = (bool (*)(const char *))dlsym(this->backend_handle,
                                                                       "adaptyst_module_process");

    if (!process_func) {
      this->throw_error("Backend \"" + this->backend + "\" doesn't define adaptyst_module_process()! "
                        "Has it been compiled correctly?");
    }

    this->process_future = std::async([process_func, sdfg]() {
      return process_func(sdfg.c_str());
    });
  }

  bool Node::wait() {
    bool result = this->process_future.get();

    if (!result && !this->error.empty()) {
      this->throw_error(this->error);
    }

    return result;
  }

  void Node::close() {
    void (*close)() = (void (*)())dlsym(this->backend_handle,
                                        "adaptyst_module_close");

    if (!close) {
      this->throw_error("Backend \"" + this->backend + "\" doesn't define adaptyst_module_close()! "
                        "Has it been compiled correctly?");
    }

    close();
  }

  void Node::set_will_profile(bool will_profile) {
    this->will_profile = will_profile;
  }

  bool Node::get_will_profile() {
    return this->will_profile;
  }

  void Node::set_error(std::string error) {
    this->error = error;
  }

  void Node::set_context(void *context) {
    this->context = context;
  }

  void *Node::get_context() {
    return this->context;
  }

  std::unordered_map<std::string, option> &Node::get_options() {
    return this->options;
  }

  std::unique_ptr<Path> &Node::get_node_dir() {
    return this->node_dir;
  }

  std::unordered_set<std::string> &Node::get_tags() {
    return this->tags;
  }

  std::unordered_map<std::string, Node::OptionMetadata> &Node::get_all_options() {
    return this->option_metadata;
  }

  void Node::set_node_dir(fs::path &node_dir) {
    this->node_dir = std::make_unique<Path>(node_dir);
  }

  void Node::profile_notify() {
    // TODO
  }

  int Node::profile_wait() {
    // TODO
    return 0;
  }

  void Node::add_in_tags(std::unordered_set<std::string> &tags) {
    // TODO
  }

  void Node::add_out_tags(std::unordered_set<std::string> &tags) {
    // TODO
  }

  bool Node::has_in_tag(std::string tag) {
    // TODO
    return false;
  }

  bool Node::has_out_tag(std::string tag) {
    // TODO
    return false;
  }

  std::vector<std::string> Node::get_log_types() {
    return this->log_types;
  }

  std::string Node::get_type() {
    return "Node";
  }

  NodeConnection::NodeConnection(std::string id,
                                 std::shared_ptr<Node> &departure_node,
                                 std::shared_ptr<Node> &arrival_node) : Identifiable(id) {
    this->departure_node = departure_node;
    this->arrival_node = arrival_node;
  }

  std::shared_ptr<Node> &NodeConnection::get_departure_node() {
    return this->departure_node;
  }

  std::shared_ptr<Node> &NodeConnection::get_arrival_node() {
    return this->arrival_node;
  }

  std::vector<std::string> NodeConnection::get_log_types() {
    return {};
  }

  std::string NodeConnection::get_type() {
    return "Connection";
  }

  Entity::Entity(std::string id, AccessMode access_mode,
                 unsigned int processing_threads,
                 fs::path local_config_path,
                 fs::path tmp_dir) : Identifiable(id) {
    this->access_mode = access_mode;
    this->processing_threads = processing_threads;
    this->local_config_path = local_config_path;
    this->tmp_dir = tmp_dir;
  }

  void Entity::add_node(std::shared_ptr<Node> &node) {
    this->nodes[node->get_id()] = node;
  }

  void Entity::add_connection(std::string id,
                              std::string departure_node,
                              std::string arrival_node) {
    if (this->connections.find(id) != this->connections.end()) {
      this->throw_error("A connection with ID \"" + id + "\" already exists!");
    }

    this->connections[id] =
      std::make_shared<NodeConnection>(id, this->get_node(departure_node),
                                   this->get_node(arrival_node));
  }

  std::shared_ptr<Node> &Entity::get_node(std::string id) {
    if (this->nodes.find(id) == this->nodes.end()) {
      this->throw_error("Node \"" + id + "\" does not exist!");
    }

    return this->nodes[id];
  }

  void Entity::set_directing_node(std::string node) {
    this->directing_node = node;
  }

  std::string Entity::get_directing_node() {
    return this->directing_node;
  }

  profile_info &Entity::get_profile_info() {
    return this->profiling_info;
  }

  void Entity::set_profile_info(profile_info &info) {
    this->profiling_info = info;
  }

  void Entity::init() {
    for (auto entry : this->nodes) {
      entry.second->init();

      if (entry.second->get_will_profile()) {
        this->will_profile = true;
      }
    }
  }

  void Entity::process(bool save_src_code_paths) {
    if (this->will_profile && this->access_mode != CUSTOM &&
        this->access_mode != CUSTOM_REMOTE) {
      if (this->access_mode == REMOTE) {
        py::initialize_interpreter();
      }

      fs::path sdfg_lib_path = this->get_tmp_dir() / "root_sdfg.so";

      try {
        py::module_ compile_sdfg = py::module_::import("compile_sdfg");
        compile_sdfg.attr("compile")(this->sdfg,
                                     sdfg_lib_path.string());
      } catch (py::error_already_set &e) {
        throw e;
      }

      if (this->access_mode == REMOTE) {
        py::finalize_interpreter();
      }

      this->profiled_process = std::make_unique<Process>([sdfg_lib_path]() {
        void *handle = dlopen(sdfg_lib_path.c_str(), RTLD_LAZY);

        if (!handle) {
          return 100;
        }

        void *(*init)() = (void *(*)()) dlsym(handle, "__dace_init_AdaptystRootSDFG");

        if (!init) {
          return 101;
        }

        void *sdfg_handle = init();

        void (*program)(void *) = (void (*)(void *)) dlsym(handle, "__program_AdaptystRootSDFG");

        if (!program) {
          return 102;
        }

        program(sdfg_handle);

        int (*exit)(void *) = (int (*)(void *)) dlsym(handle, "__dace_exit_AdaptystRootSDFG");

        if (!exit) {
          return 103;
        }

        exit(sdfg_handle);

        return 0;
      });

      fs::path stdout_path =
        fs::path(Terminal::instance->get_log_dir()) / (this->get_id() + "_stdout.log");
      fs::path stderr_path =
        fs::path(Terminal::instance->get_log_dir()) / (this->get_id() + "_stderr.log");

      this->profiled_process->set_redirect_stdout(stdout_path);
      this->profiled_process->set_redirect_stderr(stderr_path);

      this->profiling_info.type = LINUX_PROCESS;
      this->profiling_info.data.pid = this->profiled_process->start(
          true, CPUConfig(this->get_cpu_mask()), false);
    }

    for (auto entry : this->nodes) {
      entry.second->process(this->sdfg);
    }

    for (auto entry : this->nodes) {
      entry.second->wait();
    }

    this->profile_wait();

    if (save_src_code_paths) {
      Archive archive(fs::path(this->entity_dir->get_path_name()) / "src.zip");
      nlohmann::json src_mapping = nlohmann::json::object();

      for (auto &path : this->src_code_paths) {
        if (fs::exists(path)) {
          std::string filename =
            std::to_string(src_mapping.size()) + path.extension().string();
          src_mapping[path.string()] = filename;
          archive.add_file(filename, path);
        }
      }

      std::string src_mapping_str = nlohmann::to_string(src_mapping) + '\n';
      std::stringstream s;
      s << src_mapping_str;
      archive.add_file_stream("index.json", s, src_mapping_str.length());
      archive.close();
    }
  }

  void Entity::close() {
    for (auto entry : this->nodes) {
      entry.second->close();
    }
  }

  void Entity::set_entity_dir(fs::path &entity_dir) {
    this->entity_dir = std::make_unique<Path>(entity_dir);

    for (auto node : this->nodes) {
      fs::path node_dir = entity_dir / node.first;
      node.second->set_node_dir(node_dir);
    }
  }

  void Entity::profile_notify() {
    if (this->profiled_process) {
      this->profiled_process->notify();
    }
  }

  int Entity::profile_wait() {
    if (this->profiled_process) {
      return this->profiled_process->join();
    }

    return -1;
  }

  const char *Entity::get_cpu_mask() {
    if (!this->cpu_mask.empty()) {
      return this->cpu_mask.c_str();
    }

    Terminal::instance->print("The CPU mask has been requested, calculating it...", false,
                              false, this, "General");

    int num_proc = std::thread::hardware_concurrency();

    if (num_proc == 0) {
      Terminal::instance->print("Could not determine the number "
                                "of available logical cores!", true, true, this, "General");
      return nullptr;
    }

    std::stringstream mask_stream;

    if (processing_threads == 0) {
      for (int i = 0; i < num_proc; i++) {
        mask_stream << 'b';
      }

      this->cpu_mask = mask_stream.str();
    } else if (processing_threads > num_proc - 3) {
      Terminal::instance->print("The value of \"processing_threads\" must be less "
                                "than or equal to the number of "
                                "logical cores minus 3 (i.e. " +
                                std::to_string(num_proc - 3) + ")!", true, true, this, "General");
      return nullptr;
    } else {
      if (num_proc < 4) {
        Terminal::instance->print("Because there are fewer than 4 logical cores, "
                                  "the value of \"processing_threads\" will be ignored for the profiled "
                                  "program unless it is 0.", true, false);
      }

      if (fork() == 0) {
        char * const a[] = { const_cast<char *>("a"), NULL };
        execvp("test", a);
      }

      switch (num_proc) {
      case 1:
        Terminal::instance->print("Running analysis along with processing is *NOT* "
                                  "recommended on a machine with only one logical core! "
                                  "You are very likely to get inconsistent results due "
                                  "to processing threads interfering with the analysed "
                                  "program. If you want to proceed anyway, "
                                  "set \"processing_threads\" to 0.", true, true, this, "General");
        return nullptr;

      case 2:
        Terminal::instance->print("2 logical cores detected, running processing and "
                                  "hardware modules on core #0 and the command on core #1.", true,
                                  false, this, "General");
        this->cpu_mask = "pc";
        break;

      case 3:
        Terminal::instance->print("3 logical cores detected, running processing and "
                                  "hardware modules on cores #0 and #1 and the command on core #2.",
                                  true, false, this, "General");
        this->cpu_mask = "ppc";
        break;

      default:
        mask_stream << "  ";
        for (int i = 2; i < 2 + processing_threads; i++) {
          mask_stream << 'p';
        }
        for (int i = 2 + processing_threads; i < num_proc; i++) {
          mask_stream << 'c';
        }
        this->cpu_mask = mask_stream.str();
        break;
      }
    }

    Terminal::instance->print("The CPU mask has been obtained.", false,
                              false, this, "General");
    return this->cpu_mask.c_str();
  }

  fs::path Entity::get_tmp_dir() {
    return this->tmp_dir;
  }

  fs::path Entity::get_local_config_dir() {
    return this->local_config_path;
  }

  std::vector<std::shared_ptr<Identifiable>> Entity::get_all_nodes() {
    std::vector<std::shared_ptr<Identifiable> > result;

    for (auto &node : this->nodes) {
      result.push_back(node.second);
    }

    return result;
  }

  std::vector<std::string> Entity::get_log_types() {
    return {"General", "stdout", "stderr"};
  }

  std::string Entity::get_type() {
    return "Entity";
  }

  void Entity::set_sdfg(std::string sdfg) {
    this->sdfg = sdfg;
  }

  void Entity::add_src_code_path(fs::path path) {
    this->src_code_paths.insert(path);
  }

  std::unordered_set<fs::path> &Entity::get_src_code_paths() {
    return this->src_code_paths;
  }

  void System::init(fs::path def_file,
                    fs::path root_dir,
                    fs::path library_path,
                    fs::path local_config_path,
                    fs::path tmp_dir) {
    ryml::Tree tree;

    {
      std::ifstream yaml(def_file);
      std::string yaml_str((std::istreambuf_iterator<char>(yaml)),
                            std::istreambuf_iterator<char>());
      tree = ryml::parse_in_arena(yaml_str.c_str());
    }

    auto root = tree.rootref();

    if (!root.is_map()) {
       throw std::runtime_error("The system YAML file is not a map!");
    }

    if (!root.has_child("entities")) {
       throw std::runtime_error("The system YAML file does not have "
                               "\"entities\" in its root!");
    }

    auto entities = root["entities"];

    if (!entities.is_map()) {
      throw std::runtime_error("\"entities\" in the system YAML file "
                               "is not a map!");
    }

    for (auto entity : entities.children()) {
      std::string name(entity.key().data(), entity.key().len);

      if (!entity.is_map()) {
        throw std::runtime_error("\"" + name + "\" in \"entities\" in "
                                 "the system YAML file is not a map!");
      }

      if (!entity.has_child("options")) {
        throw std::runtime_error("\"" + name + "\" in \"entities\" "
                                 "in the system YAML file does not have "
                                 "\"options\"!");
      }

      auto options = entity["options"];

      if (!options.is_map()) {
        throw std::runtime_error("\"options\" in \"" + name + "\" in "
                                 "\"entities\" in the system YAML file "
                                 "is not a map!");
      }

      if (!options.has_child("access_mode")) {
        throw std::runtime_error("\"options\" in \"" + name + "\" in "
                                 "\"entities\" in the system YAML file "
                                 "does not have \"access_mode\"!");
      }


      auto access_mode = options["access_mode"];

      if (!access_mode.is_keyval()) {
        throw std::runtime_error("\"access_mode\" in \"options\" in "
                                 "\"" + name + "\" in \"entities\" "
                                 "in the system YAML file is not of "
                                 "simple key-value type!");
      }

      unsigned int processing_threads = 1;

      if (options.has_child("processing_threads")) {
        auto threads = options["processing_threads"];
        if (!threads.is_keyval()) {
          throw std::runtime_error("\"processing_threads\" in \"options\" in "
                                   "\"" + name + "\" in \"entities\" "
                                   "in the system YAML file is not of "
                                   "simple key-value type!");
        }

        std::string threads_str(threads.val().data(), threads.val().len);

        try {
          processing_threads = std::stoul(threads_str);
        } catch (...) {
          throw std::runtime_error("\"processing_threads\" in \"options\" in "
                                   "\"" + name + "\" in \"entities\" "
                                   "in the system YAML file is not a valid "
                                   "unsigned integer!");
        }
      }

      std::string access_mode_val(access_mode.val().data(), access_mode.val().len);
      Entity::AccessMode access_mode_final;

      if (access_mode_val == "in_place") {
        access_mode_final = Entity::IN_PLACE;
      } else if (access_mode_val == "remote") {
        throw std::runtime_error("Remote access to entities is not "
                                 "yet supported! (entity \"" + name + "\")");
      } else if (access_mode_val == "custom") {
        access_mode_final = Entity::CUSTOM;
      } else if (access_mode_val == "custom_remote") {
        throw std::runtime_error("Remote access to entities is not "
                                 "yet supported! (entity \"" + name + "\")");
      } else {
        throw std::runtime_error("\"access_mode\" in \"options\" in "
                                 "\"" + name + "\" in \"entities\" "
                                 "in the system YAML file has an "
                                 "invalid value! " + access_mode_val);
      }

      std::shared_ptr<Entity> entity_obj =
        std::make_shared<Entity>(name, access_mode_final,
                                 processing_threads,
                                 local_config_path,
                                 tmp_dir);

      if (!entity.has_child("nodes")) {
        throw std::runtime_error("\"" + name + "\" in \"entities\" in "
                                 "the system YAML file does not have "
                                 "\"nodes\"!");
      }

      auto nodes = entity["nodes"];

      if (!nodes.is_map()) {
        throw std::runtime_error("\"nodes\" in \"" + name + "\" in "
                                 "\"entities\" in the system YAML file "
                                 "is not a map!");
      }

      for (auto node : nodes.children()) {
        std::string node_name(node.key().data(), node.key().len);
        if (!node.is_map()) {
          throw std::runtime_error("Node \"" + node_name + "\" in entity "
                                   "\"" + name + "\" in the system YAML file "
                                   "is not a map!");
        }

        if (!node.has_child("backend")) {
          throw std::runtime_error("Node \"" + node_name + "\" in entity "
                                   "\"" + name + "\" in the system YAML file "
                                   "does not have \"backend\"!");
        }

        auto backend = node["backend"];

        if (!backend.is_keyval()) {
          throw std::runtime_error("\"backend\" in node \"" + node_name + "\" in "
                                   "entity \"" + name + "\" in the system YAML file "
                                   "is not of simple key-value form!");
        }

        std::string backend_str(backend.val().data(), backend.val().len);

        std::unordered_map<std::string, std::string> options_map;
          std::unordered_map<std::string, std::vector<std::string> > array_options_map;

        if (node.has_child("options")) {
          auto options = node["options"];

          if (!options.is_map()) {
            throw std::runtime_error("\"options\" in node \"" + node_name + "\" in "
                                     "entity \"" + name + "\" in the system YAML file "
                                     "is not a map!");
          }

          for (auto option : options.children()) {
            std::string key(option.key().data(), option.key().len);

            if (option.is_keyval()) {
              options_map[key] = std::string(option.val().data(), option.val().len);
            } else if (option.is_seq()) {
              array_options_map[key] = std::vector<std::string>();
              for (auto element : option.children()) {
                if (!element.is_val()) {
                  throw std::runtime_error("Element with index " +
                                           std::to_string(array_options_map[key].size()) +
                                           " in option \"" + key + "\" in node \"" +
                                           node_name + "\" in entity \"" + name + "\" in "
                                           "the system YAML file is not a simple value!");
                }

                array_options_map[key].push_back(std::string(element.val().data(), element.val().len));
              }
            }
          }
        }

        std::shared_ptr<Node> node_obj = std::make_shared<Node>(node_name,
                                                                backend_str,
                                                                options_map,
                                                                array_options_map,
                                                                entity_obj,
                                                                library_path);
        entity_obj->add_node(node_obj);
      }

      if (entity.has_child("edges")) {
        auto edges = entity["edges"];

        if (!edges.is_map()) {
          throw std::runtime_error("\"edges\" in \"" + name + "\" in "
                                   "\"entities\" in the system YAML file "
                                   "is not a map!");
        }

        for (auto edge : edges.children()) {
          std::string edge_name(edge.key().data(), edge.key().len);

          if (!edge.is_map()) {
            throw std::runtime_error("\"" + edge_name + "\" in \"edges\" in "
                                     "\"" + name + "\" in \"entities\" in the "
                                     "system YAML file is not a map!");
          }

          if (!edge.has_child("path")) {
            throw std::runtime_error("Edge \"" + edge_name + "\" in entity \"" +
                                     name + "\" in the system YAML file does not "
                                     "have \"path\"!");
          }

          auto path = edge["path"];

          if (!path.is_seq()) {
            throw std::runtime_error("\"path\" in edge \"" + edge_name + "\" in "
                                     "entity \"" + name + "\" in the system YAML "
                                     "file is not an array!");
          }

          if (path.num_children() != 2) {
            throw std::runtime_error("\"path\" in edge \"" + edge_name + "\" in "
                                     "entity \"" + name + "\" in the system YAML "
                                     "file does not have exactly 2 elements!");
          }

          auto node1 = path[0];
          auto node2 = path[1];

          if (!node1.is_val()) {
            throw std::runtime_error("First element in \"path\" in edge \"" +
                                     edge_name + "\" in entity \"" + name + "\" "
                                     "in the system YAML file is not a simple "
                                     "value!");
          }

          if (!node2.is_val()) {
            throw std::runtime_error("Second element in \"path\" in edge \"" +
                                     edge_name + "\" in entity \"" + name +
                                     "\" "
                                     "in the system YAML file is not a simple "
                                     "value!");
          }

          std::shared_ptr<NodeConnection> connection =
            std::make_shared<
              NodeConnection>(edge_name, entity_obj->get_node(std::string(node1.val().data(), node1.val().len)),
                              entity_obj->get_node(std::string(node2.val().data(), node2.val().len)));
        }
      }

      this->entities[name] = entity_obj;
    }

    this->root_dir = std::make_unique<Path>(root_dir);

    try {
      if (!fs::copy_file(def_file, root_dir / "system.yml")) {
        throw std::runtime_error("Could not copy the system definition file "
                                 "to the output directory!");
      }
    } catch (fs::filesystem_error &e) {
      throw std::runtime_error("Could not copy the system definition file "
                               "to the output directory! Error details: " +
                               std::string(e.what()));
    }

    for (auto &entity : this->entities) {
      fs::path entity_dir = root_dir / entity.first;
      entity.second->set_entity_dir(entity_dir);
      entity.second->init();
    }
  }

  System::System(fs::path def_file,
                 fs::path root_dir,
                 fs::path library_path,
                 fs::path local_config_path,
                 fs::path tmp_dir) {
    this->init(def_file, root_dir, library_path,
               local_config_path, tmp_dir);
    this->custom_src_code_paths_save = false;
  }

  System::System(fs::path def_file,
                 fs::path root_dir,
                 fs::path library_path,
                 fs::path local_config_path,
                 fs::path tmp_dir,
                 std::variant<fs::path, int> codes_dst) {
    this->init(def_file, root_dir, library_path,
               local_config_path, tmp_dir);
    this->custom_src_code_paths_save = true;
    this->codes_dst = codes_dst;
  }

  System::~System() {
    for (auto &entity : this->entities) {
      entity.second->close();
    }
  }

  std::vector<std::shared_ptr<Identifiable> > System::get_all_identifiables() {
    std::vector<std::shared_ptr<Identifiable> > result;
    for (auto &entity : this->entities) {
      result.push_back(entity.second);
      for (auto &node : entity.second->get_all_nodes()) {
        result.push_back(node);
      }
    }

    return result;
  }

  void System::set_sdfg(std::string sdfg) {
    for (auto &entity : this->entities) {
      entity.second->set_sdfg(sdfg);
    }
  }

  void System::process() {
    for (auto &entity : this->entities) {
      entity.second->process(!this->custom_src_code_paths_save);
    }

    if (this->custom_src_code_paths_save) {
      if (this->codes_dst.index() == 0) {
        fs::path src_code_paths_file = std::get<0>(this->codes_dst);

        std::ofstream src_code_paths_stream(src_code_paths_file);

        if (!src_code_paths_stream) {
          throw std::runtime_error("Could not open " +
                                   src_code_paths_file.string() +
                                   " for writing!");
        }

        for (auto &entry : this->entities) {
          for (auto &path : entry.second->get_src_code_paths()) {
            src_code_paths_stream << path.string() << std::endl;
          }
        }
      } else {
        int write_fd[2] = {-1, std::get<1>(this->codes_dst)};
        FileDescriptor fd(nullptr, write_fd, 1024);

        for (auto &entry : this->entities) {
          for (auto &path : entry.second->get_src_code_paths()) {
            fd.write(path.string(), true);
          }
        }
      }
    }
  }
};
