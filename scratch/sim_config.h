#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include "json.hpp"

struct SimConfig {
  // --- files ---
  std::string topology_file, flow_file, trace_file, trace_output_file;
  std::string fct_output_file = "fct.txt";
  std::string pfc_output_file = "pfc.txt";
  std::string qlen_mon_file;

  // --- switch ---
  bool enable_qcn = true;
  bool use_dynamic_pfc_threshold = true;
  uint32_t buffer_size = 16;
  int headroom_factor = 3;

  // --- packet ---
  uint32_t packet_payload_size = 1000;
  uint32_t l2_chunk_size = 0;
  uint32_t l2_ack_interval = 0;
  bool l2_back_to_zero = false;
  double error_rate_per_link = 0.0;
  double pause_time = 5;

  // --- congestion_control ---
  uint32_t cc_mode = 1;
  bool clamp_target_rate = false;
  double alpha_resume_interval = 55;
  double rp_timer = 0;
  double ewma_gain = 1.0 / 16;
  uint32_t fast_recovery_times = 5;
  std::string rate_ai, rate_hai;
  std::string min_rate = "100Mb/s";
  std::string dctcp_rate_ai = "1000Mb/s";
  double rate_decrease_interval = 4;
  uint32_t has_win = 1;
  uint32_t global_t = 1;
  uint32_t mi_thresh = 5;
  bool var_win = false;
  bool fast_react = true;
  bool multi_rate = true;
  bool sample_feedback = false;
  double pint_log_base = 1.05;
  double pint_prob = 1.0;
  double u_target = 0.95;
  uint32_t int_multi = 1;
  bool rate_bound = true;
  int nic_total_pause_time = 0;
  uint32_t ack_high_prio = 0;

  // --- simulator ---
  double simulator_stop_time = 3.01;

  // --- link ---
  uint64_t link_down_time = 0;
  uint32_t link_down_A = 0, link_down_B = 0;

  // --- trace ---
  uint32_t enable_trace = 1;

  // --- ecn ---
  std::unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;
  std::unordered_map<uint64_t, double> rate2pmax;

  // --- queue_monitor ---
  uint64_t qlen_mon_start = 0, qlen_mon_end = 2100000000;
  uint32_t qlen_mon_interval = 100;
};

inline bool ReadConf(const std::string &network_configuration, SimConfig &cfg) {
  using json = nlohmann::json;

  std::ifstream conf(network_configuration);
  if (!conf.is_open()) {
    std::cout << "Error: cannot find network config file: "
              << network_configuration << std::endl;
    fflush(stdout);
    return false;
  }

  json j;
  try {
    j = json::parse(conf);
  } catch (const json::parse_error &e) {
    std::cout << "Error: failed to parse JSON config: " << e.what()
              << std::endl;
    fflush(stdout);
    return false;
  }
  conf.close();

  if (j.contains("files")) {
    auto &f = j["files"];
    if (f.contains("topology_file"))
      cfg.topology_file = f["topology_file"].get<std::string>();
    if (f.contains("flow_file"))
      cfg.flow_file = f["flow_file"].get<std::string>();
    if (f.contains("trace_file"))
      cfg.trace_file = f["trace_file"].get<std::string>();
    if (f.contains("trace_output_file"))
      cfg.trace_output_file = f["trace_output_file"].get<std::string>();
    if (f.contains("fct_output_file"))
      cfg.fct_output_file = f["fct_output_file"].get<std::string>();
    if (f.contains("pfc_output_file"))
      cfg.pfc_output_file = f["pfc_output_file"].get<std::string>();
    if (f.contains("qlen_mon_file"))
      cfg.qlen_mon_file = f["qlen_mon_file"].get<std::string>();
  }

  if (j.contains("switch")) {
    auto &s = j["switch"];
    if (s.contains("enable_qcn"))
      cfg.enable_qcn = s["enable_qcn"].get<bool>();
    if (s.contains("use_dynamic_pfc_threshold"))
      cfg.use_dynamic_pfc_threshold =
          s["use_dynamic_pfc_threshold"].get<bool>();
    if (s.contains("buffer_size"))
      cfg.buffer_size = s["buffer_size"].get<uint32_t>();
    if (s.contains("headroom_factor")) {
      cfg.headroom_factor = s["headroom_factor"].get<int>();
      std::cout << "headroom factor set to: " << cfg.headroom_factor
                << std::endl;
    }
  }

  if (j.contains("packet")) {
    auto &p = j["packet"];
    if (p.contains("payload_size"))
      cfg.packet_payload_size = p["payload_size"].get<uint32_t>();
    if (p.contains("l2_chunk_size"))
      cfg.l2_chunk_size = p["l2_chunk_size"].get<uint32_t>();
    if (p.contains("l2_ack_interval"))
      cfg.l2_ack_interval = p["l2_ack_interval"].get<uint32_t>();
    if (p.contains("l2_back_to_zero"))
      cfg.l2_back_to_zero = p["l2_back_to_zero"].get<bool>();
    if (p.contains("error_rate_per_link"))
      cfg.error_rate_per_link = p["error_rate_per_link"].get<double>();
    if (p.contains("pause_time"))
      cfg.pause_time = p["pause_time"].get<double>();
  }

  if (j.contains("congestion_control")) {
    auto &cc = j["congestion_control"];
    if (cc.contains("cc_mode"))
      cfg.cc_mode = cc["cc_mode"].get<uint32_t>();
    if (cc.contains("clamp_target_rate"))
      cfg.clamp_target_rate = cc["clamp_target_rate"].get<bool>();
    if (cc.contains("alpha_resume_interval"))
      cfg.alpha_resume_interval = cc["alpha_resume_interval"].get<double>();
    if (cc.contains("rp_timer"))
      cfg.rp_timer = cc["rp_timer"].get<double>();
    if (cc.contains("ewma_gain"))
      cfg.ewma_gain = cc["ewma_gain"].get<double>();
    if (cc.contains("fast_recovery_times"))
      cfg.fast_recovery_times = cc["fast_recovery_times"].get<uint32_t>();
    if (cc.contains("rate_ai"))
      cfg.rate_ai = cc["rate_ai"].get<std::string>();
    if (cc.contains("rate_hai"))
      cfg.rate_hai = cc["rate_hai"].get<std::string>();
    if (cc.contains("min_rate"))
      cfg.min_rate = cc["min_rate"].get<std::string>();
    if (cc.contains("dctcp_rate_ai"))
      cfg.dctcp_rate_ai = cc["dctcp_rate_ai"].get<std::string>();
    if (cc.contains("rate_decrease_interval"))
      cfg.rate_decrease_interval = cc["rate_decrease_interval"].get<double>();
    if (cc.contains("has_win"))
      cfg.has_win = cc["has_win"].get<uint32_t>();
    if (cc.contains("global_t"))
      cfg.global_t = cc["global_t"].get<uint32_t>();
    if (cc.contains("mi_thresh"))
      cfg.mi_thresh = cc["mi_thresh"].get<uint32_t>();
    if (cc.contains("var_win"))
      cfg.var_win = cc["var_win"].get<bool>();
    if (cc.contains("fast_react"))
      cfg.fast_react = cc["fast_react"].get<bool>();
    if (cc.contains("u_target"))
      cfg.u_target = cc["u_target"].get<double>();
    if (cc.contains("int_multi"))
      cfg.int_multi = cc["int_multi"].get<uint32_t>();
    if (cc.contains("rate_bound"))
      cfg.rate_bound = cc["rate_bound"].get<bool>();
    if (cc.contains("multi_rate"))
      cfg.multi_rate = cc["multi_rate"].get<bool>();
    if (cc.contains("sample_feedback"))
      cfg.sample_feedback = cc["sample_feedback"].get<bool>();
    if (cc.contains("pint_log_base"))
      cfg.pint_log_base = cc["pint_log_base"].get<double>();
    if (cc.contains("pint_prob"))
      cfg.pint_prob = cc["pint_prob"].get<double>();
    if (cc.contains("nic_total_pause_time"))
      cfg.nic_total_pause_time = cc["nic_total_pause_time"].get<int>();
    if (cc.contains("ack_high_prio"))
      cfg.ack_high_prio = cc["ack_high_prio"].get<uint32_t>();
  }

  if (j.contains("simulator")) {
    auto &sim = j["simulator"];
    if (sim.contains("stop_time"))
      cfg.simulator_stop_time = sim["stop_time"].get<double>();
  }

  if (j.contains("link")) {
    auto &lk = j["link"];
    if (lk.contains("link_down_time"))
      cfg.link_down_time = lk["link_down_time"].get<uint64_t>();
    if (lk.contains("link_down_a"))
      cfg.link_down_A = lk["link_down_a"].get<uint32_t>();
    if (lk.contains("link_down_b"))
      cfg.link_down_B = lk["link_down_b"].get<uint32_t>();
  }

  if (j.contains("trace")) {
    auto &tr = j["trace"];
    if (tr.contains("enable_trace"))
      cfg.enable_trace = tr["enable_trace"].get<bool>() ? 1 : 0;
  }

  if (j.contains("ecn")) {
    auto &ecn = j["ecn"];
    if (ecn.contains("kmax_map")) {
      for (auto &[key, val] : ecn["kmax_map"].items())
        cfg.rate2kmax[std::stoull(key)] = val.get<uint32_t>();
    }
    if (ecn.contains("kmin_map")) {
      for (auto &[key, val] : ecn["kmin_map"].items())
        cfg.rate2kmin[std::stoull(key)] = val.get<uint32_t>();
    }
    if (ecn.contains("pmax_map")) {
      for (auto &[key, val] : ecn["pmax_map"].items())
        cfg.rate2pmax[std::stoull(key)] = val.get<double>();
    }
  }

  if (j.contains("queue_monitor")) {
    auto &qm = j["queue_monitor"];
    if (qm.contains("start"))
      cfg.qlen_mon_start = qm["start"].get<uint64_t>();
    if (qm.contains("end"))
      cfg.qlen_mon_end = qm["end"].get<uint64_t>();
  }

  fflush(stdout);
  return true;
}

#endif // SIM_CONFIG_H
