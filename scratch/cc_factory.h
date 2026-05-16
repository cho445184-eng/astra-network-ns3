#ifndef CC_FACTORY_H
#define CC_FACTORY_H

#include "sim_config.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include <ns3/rdma-cc-dcqcn.h>
#include <ns3/rdma-cc-dctcp.h>
#include <ns3/rdma-cc-hpcc-pint.h>
#include <ns3/rdma-cc-hpcc.h>
#include <ns3/rdma-cc-timely.h>
#include <ns3/rdma-congestion-ops.h>

using namespace ns3;

inline Ptr<RdmaCongestionOps>
CreateCongestionControlOps(const SimConfig &cfg) {
  if (cfg.cc_mode == 1) {
    Ptr<RdmaCcDcqcn> ops = CreateObject<RdmaCcDcqcn>();
    ops->SetAttribute("ClampTargetRate", BooleanValue(cfg.clamp_target_rate));
    ops->SetAttribute("AlphaResumeInterval",
                      DoubleValue(cfg.alpha_resume_interval));
    ops->SetAttribute("RPTimer", DoubleValue(cfg.rp_timer));
    ops->SetAttribute("FastRecoveryTimes",
                      UintegerValue(cfg.fast_recovery_times));
    ops->SetAttribute("EwmaGain", DoubleValue(cfg.ewma_gain));
    ops->SetAttribute("RateAI", DataRateValue(DataRate(cfg.rate_ai)));
    ops->SetAttribute("RateHAI", DataRateValue(DataRate(cfg.rate_hai)));
    ops->SetAttribute("RateDecreaseInterval",
                      DoubleValue(cfg.rate_decrease_interval));
    ops->SetAttribute("MinRate", DataRateValue(DataRate(cfg.min_rate)));
    return ops;
  } else if (cfg.cc_mode == 3) {
    Ptr<RdmaCcHpcc> ops = CreateObject<RdmaCcHpcc>();
    ops->SetAttribute("TargetUtil", DoubleValue(cfg.u_target));
    ops->SetAttribute("MiThresh", UintegerValue(cfg.mi_thresh));
    ops->SetAttribute("MultiRate", BooleanValue(cfg.multi_rate));
    ops->SetAttribute("SampleFeedback", BooleanValue(cfg.sample_feedback));
    ops->SetAttribute("FastReact", BooleanValue(cfg.fast_react));
    ops->SetAttribute("RateAI", DataRateValue(DataRate(cfg.rate_ai)));
    ops->SetAttribute("MinRate", DataRateValue(DataRate(cfg.min_rate)));
    return ops;
  } else if (cfg.cc_mode == 7) {
    Ptr<RdmaCcTimely> ops = CreateObject<RdmaCcTimely>();
    ops->SetAttribute("RateAI", DataRateValue(DataRate(cfg.rate_ai)));
    ops->SetAttribute("RateHAI", DataRateValue(DataRate(cfg.rate_hai)));
    ops->SetAttribute("MinRate", DataRateValue(DataRate(cfg.min_rate)));
    return ops;
  } else if (cfg.cc_mode == 8) {
    Ptr<RdmaCcDctcp> ops = CreateObject<RdmaCcDctcp>();
    ops->SetAttribute("EwmaGain", DoubleValue(cfg.ewma_gain));
    ops->SetAttribute("RateAI", DataRateValue(DataRate(cfg.dctcp_rate_ai)));
    ops->SetAttribute("MinRate", DataRateValue(DataRate(cfg.min_rate)));
    ops->SetAttribute("Mtu", UintegerValue(cfg.packet_payload_size));
    return ops;
  } else if (cfg.cc_mode == 10) {
    Ptr<RdmaCcHpccPint> ops = CreateObject<RdmaCcHpccPint>();
    ops->SetAttribute("TargetUtil", DoubleValue(cfg.u_target));
    ops->SetAttribute("MiThresh", UintegerValue(cfg.mi_thresh));
    ops->SetAttribute("RateAI", DataRateValue(DataRate(cfg.rate_ai)));
    ops->SetAttribute("MinRate", DataRateValue(DataRate(cfg.min_rate)));
    ops->SetSampleThreshold(cfg.pint_prob);
    return ops;
  }
  return nullptr;
}

#endif // CC_FACTORY_H
