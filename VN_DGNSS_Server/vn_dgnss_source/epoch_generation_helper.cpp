#include "epoch_generation_helper.h"

#include <iomanip>
#include <iostream>
#include <utility>

#include "beidou_code_correction.h"
#include "ssr_vtec_correction_model.h"
static void ReportDatetime(std::ostream &rst, std::vector<double> datetime) {
  rst << std::setfill('0') << std::setw(4) << (int)datetime[0] << " "
      << std::setfill('0') << std::setw(2) << (int)datetime[1] << " "
      << std::setfill('0') << std::setw(2) << (int)datetime[2] << " "
      << std::setfill('0') << std::setw(2) << (int)datetime[3] << " "
      << std::setfill('0') << std::setw(2) << (int)datetime[4] << " "
      << std::setfill('0') << std::setw(2) << (int)datetime[5] << std::endl;
}

static std::string GetSystemTypeStr(int sys) {
  if (sys == SYS_GPS) {
    return "G";
  } else if (sys == SYS_GAL) {
    return "E";
  } else if (sys == SYS_CMP) {
    return "C";
  } else {
    return "unknown";
  }
}

static int SysInforToRtcmCode(int info_code, int sys, int prn) {
  switch (sys) {
    case SYS_GPS: {
      switch (info_code) {
        case VN_CODE_GPS_C1C:
          return CODE_L1C;
        case VN_CODE_GPS_C1W:
          return CODE_L1W;
        case VN_CODE_GPS_C2C:
          return CODE_L2C;
        case VN_CODE_GPS_C2W:
          return CODE_L2W;
        case VN_CODE_GPS_C2L:
          return CODE_L2L;
        default:
          return CODE_NONE;
      }
    }
    case SYS_GAL: {
      switch (info_code) {
        case VN_CODE_GAL_C1C:
          return CODE_L1C;
        case VN_CODE_GAL_C1X:
          return CODE_L1X;
        case VN_CODE_GAL_C6C:
          return CODE_L6C;
        case VN_CODE_GAL_C5Q:
          return CODE_L5Q;
        case VN_CODE_GAL_C5X:
          return CODE_L5X;
        case VN_CODE_GAL_C7Q:
          return CODE_L7Q;
        case VN_CODE_GAL_C7X:
          return CODE_L7X;
        default:
          return CODE_NONE;
      }
    }
    case SYS_CMP: {
      switch (info_code) {
        case VN_CODE_BDS_C2I:
          return CODE_L2I;
        case VN_CODE_BDS_C6I:
          return CODE_L6I;
        case VN_CODE_BDS_C7:
          if (prn <= 18) {
            return CODE_L7I;
          } else {
            return CODE_L7Z;
          }
        default:
          return CODE_NONE;
      }
    }
    default:
      return CODE_NONE;
  }
}

EpochGenerationHelper::EpochGenerationHelper(std::vector<double> pos_ecef)
    : user_pos(std::move(pos_ecef)), date_gps(6, 0) {
  phase_windup_track.resize(3);
  ResetPhaseWindupVec();
}

EpochGenerationHelper::~EpochGenerationHelper() = default;

void EpochGenerationHelper::GetPppCorrections(BkgDataRequestor *foo_bkg,
                                              WebDataRequestor *foo_web,
                                              std::ostream &client_log) {
  // get USTEC ionospheric data
  ustec_data = foo_web->get_ustec_data();
  // get clock correction data
  clock_data = foo_bkg->GetSatClockCorrEpochs();
  // get orbit correction data
  orbit_data = foo_bkg->GetSatOrbitCorrEpochs();
  // get hardware bias
  code_bias = foo_web->get_code_bias();
  phase_bias = foo_web->get_phase_bias();
  code_bias_ssr = foo_bkg->GetSsrCodeBiasCorr();
  phase_bias_ssr = foo_bkg->GetSsrPhaseBiasCorr();
  // get ephemeris data
  eph_data = foo_bkg->GetGnssEphDataEpochs();
  vtec_ssr = foo_bkg->GetSsrVTecCorr();
}

// Find orbit data that match the selected PRN.
bool EpochGenerationHelper::SelectSatOrbitCorrection(std::ostream &rst, int prn,
                                                     int sys,
                                                     SatOrbitPara &obt_sv,
                                                     gtime_t &obt_t) {
  for (int i = 0; i < 3; i++) {
    SatOrbitPara obt_elem;
    gtime_t ssrt_sys;
    switch (sys) {
      case SYS_GPS:
        obt_elem = orbit_data[i].GPS.data_sv[prn];
        ssrt_sys = orbit_data[i].GPS.time;
        break;
      case SYS_GAL:
        obt_elem = orbit_data[i].GAL.data_sv[prn];
        ssrt_sys = orbit_data[i].GAL.time;
        break;
      case SYS_CMP:
        obt_elem = orbit_data[i].BDS.data_sv[prn];
        ssrt_sys = orbit_data[i].BDS.time;
        break;
      default:
        obt_elem.prn = -1;
    }
    if (obt_elem.prn != -1 && timediff(gpst_now, ssrt_sys) < 200.0) {
      obt_sv = obt_elem;
      obt_t = ssrt_sys;
      return true;
    }
  }
  // No data matching
  return false;
}

bool EpochGenerationHelper::SelectSatClockCorrection(std::ostream &rst, int prn,
                                                     int sys,
                                                     SatClockPara &clk_sv,
                                                     gtime_t &clk_t) {
  for (int i = 0; i < 3; i++) {
    SatClockPara clk_elem;
    gtime_t ssrt_sys;
    switch (sys) {
      case SYS_GPS:
        clk_elem = clock_data[i].GPS.data_sv[prn];
        ssrt_sys = clock_data[i].GPS.time;
        break;
      case SYS_GAL:
        clk_elem = clock_data[i].GAL.data_sv[prn];
        ssrt_sys = clock_data[i].GAL.time;
        break;
      case SYS_CMP:
        clk_elem = clock_data[i].BDS.data_sv[prn];
        ssrt_sys = clock_data[i].BDS.time;
        break;
      default:
        clk_elem.prn = -1;
    }
    if (clk_elem.prn != -1 && timediff(gpst_now, ssrt_sys) < 200.0) {
      clk_sv = clk_elem;
      clk_t = ssrt_sys;
      return true;
    }
  }
  // No data matching
  return false;
}

// Compute phase wind-up correction
void EpochGenerationHelper::ComputePhaseWindup(
    int sys_i, int prn_idx, const std::vector<double> &sat_pos_pretrans) {
  double phw, rs[3], rr[3];
  for (int i = 0; i < 3; i++) {
    rs[i] = sat_pos_pretrans[i];
    rr[i] = user_pos[i];
  }
  phw = phase_windup_track[sys_i][prn_idx];
  if (model_phw(gpst_now, 1, rs, rr, phw) == 0) {
    phw = 0;
  } else if (std::isnan(phw)) {
    printf("error: phase windup is nan");
  }
  phase_windup_track[sys_i][prn_idx] = phw;
}

void EpochGenerationHelper::ResetPhaseWindupVec() {
  phase_windup_track[0].resize(MAXPRNGPS + 1, 0);
  phase_windup_track[1].resize(MAXPRNGAL + 1, 0);
  phase_windup_track[2].resize(MAXPRNCMP + 1, 0);
}

bool EpochGenerationHelper::ConstructGnssMeas(
    BkgDataRequestor *foo_bkg, WebDataRequestor *foo_web, std::ostream &rst,
    const GnssSystemInfo &infor, const IggtropExperimentModel &TropData,
    int log_count) {
  vntimefunc::GetGpsTimeNow(date_gps, day_of_year, gpst_now);
  bool log_out = false;
  if (log_count % 60 == 1) {
    // record log by every 1 minute
    log_out = true;
  }
  if (log_out) {
    rst << "current GPS time: " << date_gps[0] << " " << date_gps[1] << " "
        << date_gps[2] << " " << date_gps[3] << " " << date_gps[4] << " "
        << date_gps[5] << std::endl;
  }
  GetPppCorrections(foo_bkg, foo_web, rst);
  double tdiff;
  /* Mute USTEC
  gtime_t t_ustec = epoch2time(ustec_data.time);
  tdiff = difftime(gpst_now.time, t_ustec.time) - 18;
  if (log_out) {
    rst << "USTEC t_diff: " << (int)(tdiff / 60) << ":"
        << tdiff - 60 * (int)(tdiff / 60) << std::endl;
    if (tdiff > 45 * 60) {
      rst << "warning: USTEC too old (>45min)." << std::endl;
    }
  }
  */

  tdiff = difftime(gpst_now.time, vtec_ssr.time.time);
  if (tdiff > 10 * 60 || !vtec_ssr.received) {
    rst << "SSR TEC t_diff: " << (int)(tdiff / 60) << ":"
        << tdiff - 60 * (int)(tdiff / 60) << std::endl;
    rst << "warning: SSR TEC too old (>10min) or not found." << std::endl;
    ResetPhaseWindupVec();
    return false;
  } else if (log_out) {
    rst << "SSR TEC t_diff: " << (int)(tdiff / 60) << ":"
        << tdiff - 60 * (int)(tdiff / 60) << std::endl;
  }

  double user_lat, user_lon, user_h = 0;
  /*  Mute USTEC
  UsTecIonoCorrComputer ido(ustec_data.data, user_pos);
  ido.GetLatLonHeight(user_lat, user_lon, user_h);
  */
  // Get LLA (Lat,Lon,H) in rad
  double LLA[3];
  ecef2pos(user_pos.data(), LLA);
  user_lat = LLA[0];
  user_lon = LLA[1];
  GeoidModelHelper geoH;
  // geodetic ellipsoidal separation, compute orthometric height of the receiver
  double Ngeo = geoH.geoidh(user_lat, user_lon);
  user_h = LLA[2] - Ngeo;

  int sys_rtklib = SYS_NONE;
  int max_prn = 0;
  // Initial time gap check
  int t_check = 0;
  if (timediff(gpst_now, clock_data[0].GPS.time) > 200 ||
      timediff(gpst_now, orbit_data[0].GPS.time) > 200) {
    if (log_out) {
      rst << "GPS SSR data too old (>200s)" << std::endl;
    }
    t_check++;
  }
  if (timediff(gpst_now, clock_data[0].GAL.time) > 200 ||
      timediff(gpst_now, orbit_data[0].GAL.time) > 200) {
    if (log_out) {
      rst << "GAL SSR data too old (>200s)" << std::endl;
    }
    t_check++;
  }
  if (timediff(gpst_now, clock_data[0].BDS.time) > 200 ||
      timediff(gpst_now, orbit_data[0].BDS.time) > 200) {
    if (log_out) {
      rst << "BDS SSR data too old (>200s)" << std::endl;
    }
    t_check++;
  }
  if (t_check == 3) {
    return false;
  }
  // Generate and initialize data struct for RTCM
  data.resize(MAXOBS);
  for (int i = 0; i < MAXOBS; i++) {
    for (int k = 0; k < NFREQ + NEXOBS; k++) {
      data[i].P[k] = data[i].L[k] = 0.0;
      data[i].D[k] = 0.0f;
      data[i].SNR[k] = data[i].LLI[k] = data[i].code[k] = 0;
      data[i].lockt[k] = 0;
    }
  }
  num_sv = 0;
  num_in_sys.resize(3, 0);
  for (int sys_i = 0; sys_i < 3; sys_i++) {
    // Checking if the corresponding system requested by client
    if (infor.sys[sys_i] && infor.code_F1[sys_i] != -1) {
      std::vector<SatBias> cbias_ftp_f1, cbias_ftp_f2;
      std::vector<SatBias> pbias_ftp_f1, pbias_ftp_f2;
      CodeBiasCorr cbias_ssr;
      PhaseBiasCorr pbias_ssr;
      std::vector<satstruct::Ephemeris> eph_sv[VN_MAX_NUM_OF_EPH_EPOCH];
      double sys_F1, sys_F2;
      switch (sys_i) {
        case 0:
          sys_rtklib = SYS_GPS;
          max_prn = MAXPRNGPS;
          cbias_ssr = code_bias_ssr.GPS;
          pbias_ssr = phase_bias_ssr.GPS;
          cbias_ftp_f1 = code_bias.bias_GPS[infor.code_F1[sys_i]];
          cbias_ftp_f2 = code_bias.bias_GPS[infor.code_F2[sys_i]];
          pbias_ftp_f1 = phase_bias.bias_GPS[infor.code_F1[sys_i]];
          pbias_ftp_f2 = phase_bias.bias_GPS[infor.code_F2[sys_i]];
          for (int j = 0; j < VN_MAX_NUM_OF_EPH_EPOCH; j++) {
            // Copy different version
            eph_sv[j] = eph_data[j].GPS_eph;
          }
          sys_F1 = FREQL1;
          sys_F2 = FREQL2;
          break;
        case 1:
          sys_rtklib = SYS_GAL;
          max_prn = MAXPRNGAL;
          cbias_ssr = code_bias_ssr.GAL;
          pbias_ssr = phase_bias_ssr.GAL;
          cbias_ftp_f1 = code_bias.bias_GAL[infor.code_F1[sys_i]];
          cbias_ftp_f2 = code_bias.bias_GAL[infor.code_F2[sys_i]];
          pbias_ftp_f1 = phase_bias.bias_GAL[infor.code_F1[sys_i]];
          pbias_ftp_f2 = phase_bias.bias_GAL[infor.code_F2[sys_i]];
          for (int j = 0; j < VN_MAX_NUM_OF_EPH_EPOCH; j++) {
            eph_sv[j] = eph_data[j].GAL_eph;
          }
          sys_F1 = FREQL1;
          sys_F2 = FREQE5b;
          break;
        case 2:
          sys_rtklib = SYS_CMP;
          max_prn = MAXPRNCMP;
          cbias_ssr = code_bias_ssr.BDS;
          pbias_ssr = phase_bias_ssr.BDS;
          cbias_ftp_f1 = code_bias.bias_BDS[infor.code_F1[sys_i]];
          cbias_ftp_f2 = code_bias.bias_BDS[infor.code_F2[sys_i]];
          pbias_ftp_f1 = phase_bias.bias_BDS[infor.code_F1[sys_i]];
          pbias_ftp_f2 = phase_bias.bias_BDS[infor.code_F2[sys_i]];
          for (int j = 0; j < VN_MAX_NUM_OF_EPH_EPOCH; j++) {
            eph_sv[j] = eph_data[j].BDS_eph;
          }
          sys_F1 = FREQ1_CMP;
          sys_F2 = FREQ2_CMP;
          break;
      }
      SatClockPara clk_sv;
      SatOrbitPara obt_sv;
      gtime_t t_clk{}, t_obt{};
      for (int prn = 1; prn < max_prn + 1; prn++) {
        if (sys_i == 2) {
          if (prn <= 5 || prn == 18 || prn >= 59) {
            if (log_out) {
              rst << GetSystemTypeStr(sys_rtklib) << prn
                  << " BDS GEO SAT ignored" << std::endl;
            }
            phase_windup_track[sys_i][prn] = 0;
            continue;
          }
        }
        // Pick SSR clock and orbit correction, check the latency
        if (!(SelectSatOrbitCorrection(rst, prn, sys_rtklib, obt_sv, t_obt) &&
              SelectSatClockCorrection(rst, prn, sys_rtklib, clk_sv, t_clk))) {
          if (log_out) {
            rst << GetSystemTypeStr(sys_rtklib) << prn << " No IGS corr"
                << std::endl;
          }
          phase_windup_track[sys_i][prn] = 0;
          continue;
        }
        // Check if code bias is available from GIPP product
        if (cbias_ftp_f1[prn].prn == -1) {
          if (log_out) {
            rst << GetSystemTypeStr(sys_rtklib) << prn
                << " No code bias corr for freq 1 from GIPP" << std::endl;
          }
          phase_windup_track[sys_i][prn] = 0;
          continue;
        }
        int ver = -1;
        for (int j = 0; j < VN_MAX_NUM_OF_EPH_EPOCH; j++) {
          // Checking for different eph version
          if (obt_sv.IOD == eph_sv[j][prn].IODE && eph_sv[j][prn].prn != -1) {
            ver = j;
            break;
          }
        }
        if (ver == -1) {
          if (log_out) {
            rst << GetSystemTypeStr(sys_rtklib) << prn
                << " IOD not match: " << obt_sv.IOD << " EPH_IOD: ";
            for (int j = 0; j < 6; j++) {
              rst << eph_sv[j][prn].IODE << " ";
            }
            rst << std::endl;
          }
          phase_windup_track[sys_i][prn] = 0;
          continue;
        }
        double eph_tdiff = timediff(gpst_now, eph_sv[ver][prn].t_oc);
        double ep[6];
        time2epoch(eph_sv[ver][prn].t_oc, ep);

        if (std::abs(eph_tdiff) > 7200.0 + 120.0 || eph_sv[ver][prn].svH != 0 ||
            eph_sv[ver][prn].prn == -1) {
          if (log_out) {
            rst << GetSystemTypeStr(sys_rtklib) << prn << "("
                << eph_sv[ver][prn].prn << ")"
                << " sv_H:" << eph_sv[ver][prn].svH
                << " time diff: " << eph_tdiff << std::endl;
          }
          phase_windup_track[sys_i][prn] = 0;
          continue;
        }
        SatPosClkComputer spco(gpst_now, obt_sv.dx_m, obt_sv.dv_m, t_obt,
                               clk_sv.dt_corr_s, t_clk, eph_sv[ver][prn],
                               sys_rtklib);
        // compute propagation time using optimization function
        spco.PropTimeOptm(user_pos);
        // Rotate satellite position
        spco.ComputePreciseOrbitClockCorrection();
        // get precise satellite position
        std::vector<double> sat_pos_precise = spco.GetPreciseSatPos();
        // get precise satellite position at its transmit time
        std::vector<double> sat_pos_pretrans = spco.GetPreSatPosAtTranst();
        // get precise satellite clock bias
        double delt_sv = spco.GetClock();
        std::vector<double> range_vector(3, 0);
        for (int j = 0; j < 3; j++) {
          range_vector[j] = user_pos[j] - sat_pos_precise[j];
        }
        double norm_range =
            sqrt(pow(range_vector[0], 2) + pow(range_vector[1], 2) +
                 pow(range_vector[2], 2));

        UsTecIonoCorrComputer ido(ustec_data.data, user_pos);
        std::vector<double> elaz =
            ido.ElevationAzimuthComputation(sat_pos_precise);
        double user_elev = elaz[0];
        if (user_elev <= ELEVMASK) {
          if (log_out) {
            rst << GetSystemTypeStr(sys_rtklib) << prn << " elev: " << user_elev
                << std::endl;
          }
          phase_windup_track[sys_i][prn] = 0;
          continue;
        }

        // compute ionospheric delay from SSR
        double iono_delay_L1 = 0, iono_delay_L2 = 0;
        SsrVtecCorrectionModel VTEC;
        iono_delay_L1 = VTEC.stec(vtec_ssr, gpst_now.sec, user_pos,
                                  sat_pos_precise, sys_F1);
        iono_delay_L2 = iono_delay_L1 * (sys_F1 * sys_F1 / (sys_F2 * sys_F2));

        // compute Tropospheric delay
        IggtropCorrectionModel IGG;
        double uLon = user_lon <= 0 ? 2 * PI + user_lon : user_lon;
        double trop_IGG =
            IGG.IGGtropdelay(uLon * R2D, user_lat * R2D, user_h / 1000,
                             day_of_year, user_elev, TropData.data);

        // COmpute Beidou code correction
        double bds_corr = 0;
        if (sys_i == 2) {
          bds_corr = beidouCodeCorrection::computeBdsCodeCorr(
              prn, user_elev * R2D, infor.code_F1[sys_i]);
        }
        // ComputePhaseWindup(sys_i, prn, sat_pos_pretrans);

        // code/phase bias product from CNE SSR
        //        BiasElement code_bias_f1 =
        //        cbias_ssr.data[prn].bias_ele[infor.code_F1[sys_i]];
        //        BiasElement phase_bias_f1 =
        //        pbias_ssr.data[prn].bias_ele[infor.code_F1[sys_i]]; if
        //        (!code_bias_f1.received) {
        //          phase_windup_track[sys_i][prn] = 0;
        //          continue;
        //        }

        if (std::isnan(norm_range)) {
          rst << "sat prc pos rotated: " << std::setprecision(13) << " "
              << sat_pos_precise[0] << " " << sat_pos_precise[1] << " "
              << sat_pos_precise[2] << std::endl;
          rst << "dx[0] ,dv[0],dt_corr[0]: " << obt_sv.dx_m[0] << " "
              << obt_sv.dv_m[0] << clk_sv.dt_corr_s[0] << std::endl;
          rst << "timediff now to ssr: " << timediff(gpst_now, t_obt) << " "
              << timediff(gpst_now, t_clk) << std::endl;
        }

        data[num_sv].sat = satno(sys_rtklib, prn);
        data[num_sv].time = gpst_now;
        // Use GIPP bias product: CLIGHT * cbias_ftp_f1[prn].value * 1e-9
        // Use CNES SSR bias product: code_bias_f1.value
        data[num_sv].P[0] = norm_range - delt_sv +
                            CLIGHT * cbias_ftp_f1[prn].value * 1e-9 +
                            iono_delay_L1 + trop_IGG - bds_corr;
        int ambiguity = 15;
        if (false && pbias_ftp_f1[prn].prn != -1) {
          data[num_sv].L[0] =
              (norm_range - delt_sv + CLIGHT * pbias_ftp_f1[prn].value * 1e-9 -
               iono_delay_L1 + trop_IGG) /
                  (CLIGHT / sys_F1) +
              ambiguity + phase_windup_track[sys_i][prn];
          // Set lock time to 0 disable phase, 1000 to enable it.
          data[num_sv].lockt[0] = 1000;
        } else {
          data[num_sv].L[0] = 0;
          data[num_sv].lockt[0] = 0;
        }
        data[num_sv].SNR[0] =
            (unsigned char)(floor(72 * user_elev / (3 * PI)) + 41) * 4;
        if (data[num_sv].SNR[0] > 200) {
          data[num_sv].SNR[0] = 200;
        }
        data[num_sv].code[0] =
            SysInforToRtcmCode(infor.code_F1[sys_i], sys_rtklib, prn);
        data[num_sv].rcv = 0;

        if (false && cbias_ftp_f2[prn].prn != -1) {
          data[num_sv].P[1] = norm_range - delt_sv +
                              CLIGHT * cbias_ftp_f2[prn].value * 1e-9 +
                              +iono_delay_L2 + trop_IGG;
          if (pbias_ftp_f2[prn].prn != -1) {
            data[num_sv].L[1] = (norm_range - delt_sv +
                                 CLIGHT * pbias_ftp_f2[prn].value * 1e-9 -
                                 iono_delay_L2 + trop_IGG) /
                                    (CLIGHT / sys_F2) +
                                ambiguity + 2 + phase_windup_track[sys_i][prn];
            data[num_sv].lockt[1] = 1000;
          } else {
            data[num_sv].L[1] = 0;
            data[num_sv].lockt[1] = 0;
          }
          data[num_sv].code[1] =
              SysInforToRtcmCode(infor.code_F2[sys_i], sys_rtklib, prn);
          data[num_sv].SNR[1] = data[num_sv].SNR[0];
        }
        if (log_out) {
          //          rst << " L2 code-phase = " << std::setprecision(5)
          //              << data[num_sv].P[1] - data[num_sv].L[1] * (CLIGHT /
          //              sys_F2)
          //              << " phase bias = " << CLIGHT *
          //              pbias_ftp_f2[prn].value * 1e-9
          //              << std::endl;
          rst << GetSystemTypeStr(sys_rtklib) << prn
              << " Eph_diff: " << std::setprecision(5) << eph_tdiff << " IODE "
              << eph_sv[ver][prn].IODE << " L1 code: "
              << std::setprecision(12)
              // << data[num_sv].P[0] << " L1 phase: " << std::setprecision(12)
              // << data[num_sv].L[0] << " L2 code: " << std::setprecision(12)
              // << data[num_sv].P[1] << " L2 phase: " << std::setprecision(12)
              << data[num_sv].L[1] << " IGGTrop: " << std::setprecision(4)
              << trop_IGG << " Iono: " << std::setprecision(5)
              << iono_delay_L1
              // << " phw: " << std::setprecision(6) <<
              // phase_windup_track[sys_i][prn]
              << std::endl;
        }
        num_in_sys[sys_i]++;
        num_sv++;
      }
    } else {
      rst << infor.sys[sys_i] << " " << infor.code_F1[sys_i] << std::endl;
    }
  }
  if (num_sv > 3) {
    if (log_out) {
      rst << "GPS: " << num_in_sys[0] << " GAL: " << num_in_sys[1]
          << " BDS: " << num_in_sys[2] << std::endl;
    }
    return true;
  } else {
    if (log_out) {
      rst << "No. of sat <= 3" << std::endl;
    }
    return false;
  }
}

void EpochGenerationHelper::SendRtcmMsgToClient(SockRTCM *client_info) {
  if (num_sv > 3) {
    int type[16];
    int m = 1; /*Number of OBS message type*/
    type[0] = 1005;
    if (num_in_sys[0] > 0) {
      type[m++] = 1074; /* GPS massage type */
    }
    if (num_in_sys[1] > 0) {
      type[m++] = 1094; /* GAL massage type */
    }
    if (num_in_sys[2] > 0) {
      type[m++] = 1124; /* BDS massage type */
    }
    CreateRtcmMsg(num_sv, type, m, client_info, user_pos, data);
  }
}
