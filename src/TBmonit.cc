// TBmonit.cc
#include "TBmonit.h"
#include "TBmid.h"
#include "TBevt.h"
#include "TBread.h"
#include "TButility.h"

#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"

#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cstdint>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

template <typename T>
TBmonit<T>::TBmonit(const std::string &fConfig_, int fRunNum_)
  : fConfig(TBconfig(fConfig_)), fRunNum(fRunNum_), fMaxEvent(-1), fMaxFile(-1), fObj(nullptr)
{
    fIsLive = false;
    fIsAux  = false;
    fUtility = TButility();
}

template <typename T>
TBmonit<T>::TBmonit(ObjectCollection* fObj_)
  : fObj(fObj_), fConfig(TBconfig("./config_general.yml"))
{
    const YAML::Node fConfig_YAML = fConfig.GetConfig();

    std::string str_version = "";
    fObj->GetVariable("version", &str_version);

    std::string mapping_path = "../mapping/mapping_TB2024_v1.root/";
    if (str_version == "2")
        mapping_path = "../mapping/mapping_TB2024_v2.root/";

    fBaseDir = fConfig_YAML["BaseDirectory"].as<std::string>();
    fUtility = TButility(mapping_path);

    fObj->GetVariable("RunNumber", &fRunNum);
    fObj->GetVariable("MaxEvent", &fMaxEvent);
    fObj->GetVariable("MaxFile", &fMaxFile);
    fObj->GetVariable("LIVE", &fIsLive);
    fObj->GetVariable("AUX", &fIsAux);

    if (fIsLive) {
        fMaxEvent = -1;
        fMaxFile  = -1;
    }
}

template <typename T>
void TBmonit<T>::GetFormattedRamInfo() {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    double total_memory_GB = static_cast<double>(memInfo.totalram) * memInfo.mem_unit / (1024.0 * 1024.0 * 1024.0);
    double free_memory_GB = static_cast<double>(memInfo.freeram) * memInfo.mem_unit / (1024.0 * 1024.0 * 1024.0);
    double used_memory_GB = total_memory_GB - free_memory_GB;
    printf("%.1f GB / %.1f GB (%.2f %%)", used_memory_GB, total_memory_GB, (used_memory_GB / total_memory_GB * 100.0));
}

template <typename T>
void TBmonit<T>::Loop() {
    if (fIsLive)
        LoopLive();
    else
        LoopAfterRun();
}

template <typename T>
void TBmonit<T>::LoopLive() {
    ANSI_CODE ANSI = ANSI_CODE();
    TBplotengine fPlotter = TBplotengine(fConfig.GetConfig()["ModuleConfig"], fRunNum, fIsLive, fUtility);

    std::string aCase;
    fObj->GetVariable("type", &aCase);
    if (aCase == "null")
        throw std::runtime_error("No case type provided");
    else
        fPlotter.SetCase(aCase);

    std::string aMethod;
    fObj->GetVariable("method", &aMethod);
    if (aMethod == "null")
        throw std::runtime_error("No method provided");
    else
        fPlotter.SetMethod(aMethod);

    if (aCase == "single") {
        std::vector<std::string> aModules;
        fObj->GetVector("module", &aModules);
        if (aModules.empty()) {
            throw std::runtime_error("No modules provided for single mode.");
        } else {
            std::vector<TBcid> aCID;
            for (size_t i = 0; i < aModules.size(); i++) {
                aCID.push_back(fUtility.GetCID(aModules.at(i)));
            }
            fPlotter.SetCID(aCID);
        }
    }

    else if (aCase == "heatmap") {
        std::vector<std::string> aModules;
        fObj->GetVector("module", &aModules);
        if (aModules.size() != 1) {
            throw std::runtime_error("Heatmap mode requires exactly one module.");
        } else {
            fPlotter.SetModule(aModules.at(0));
        }
    }

    fPlotter.init();

    TBread<TBwaveform> readerWave(
        fRunNum,
        fMaxEvent,
        fMaxFile,
        fIsLive,
        fBaseDir,
        fPlotter.GetUniqueMID()
    );

    while (true) {
        readerWave.CheckNextFileExistence();

        int iCurrentEvent = readerWave.GetCurrentEvent();
        int iMaxEvent = readerWave.GetLiveMaxEvent();
        auto time_begin = std::chrono::system_clock::now();

        for (int i = iCurrentEvent; i < iMaxEvent; i++) {
            if (i > iCurrentEvent && i % 10 == 0) {
                auto time_taken = std::chrono::system_clock::now() - time_begin;
                float percent_done = 1.0f * (i - iCurrentEvent) / (iMaxEvent - iCurrentEvent);
                auto time_left = time_taken * (1.0f / percent_done - 1.0f);
                auto minutes_left = std::chrono::duration_cast<std::chrono::minutes>(time_left);
                auto seconds_left = std::chrono::duration_cast<std::chrono::seconds>(time_left - minutes_left);
                std::cout << "\r\033[F" << " " << i << " / " << iMaxEvent << " events  "
                          << minutes_left.count() << ":";
                printf("%02d left (%.1f %%) | ", (int)seconds_left.count(), percent_done * 100);
                GetFormattedRamInfo();
                std::cout << ANSI.END << std::endl;
            }

            TBevt<TBwaveform> aEvent;
            fPlotter.Fill(readerWave.GetAnEvent());
        }
        fPlotter.Update();
    }
}

template <typename T>
void TBmonit<T>::LoopAfterRun() {
    ANSI_CODE ANSI = ANSI_CODE();
    TBplotengine fPlotter = TBplotengine(fConfig.GetConfig()["ModuleConfig"], fRunNum, fIsLive, fUtility);


    std::string aCase;
    fObj->GetVariable("type", &aCase);
    if (aCase == "null")
        throw std::runtime_error("No case type provided");
    else
        fPlotter.SetCase(aCase);

    std::string aMethod;
    fObj->GetVariable("method", &aMethod);
    if (aMethod == "null")
        throw std::runtime_error("No method provided");
    else
        fPlotter.SetMethod(aMethod);


    if (aCase == "single") {
        std::vector<std::string> aModules;
        fObj->GetVector("module", &aModules);
        if (aModules.empty()) {
            throw std::runtime_error("No modules provided for single mode.");
        } else {
            std::vector<TBcid> aCID;
            for (size_t i = 0; i < aModules.size(); i++) {
                aCID.push_back(fUtility.GetCID(aModules.at(i)));
            }
            fPlotter.SetCID(aCID);
        }
    }

    else if (aCase == "heatmap") {
        std::string str_version = "";
        fObj->GetVariable("version", &str_version);
        if (str_version != "1" && str_version != "2") {
            throw std::runtime_error("Invalid version for heatmap mode.");
        } else {
            fPlotter.SetModule(str_version);
        }
    }

    fPlotter.init();

    TBread<TBwaveform> readerWave(
        fRunNum,
        fMaxEvent,
        fMaxFile,
        fIsLive,
        fBaseDir,
        fPlotter.GetUniqueMID()
    );

    if (fMaxEvent == -1)
        fMaxEvent = readerWave.GetMaxEvent();
    if (fMaxEvent > readerWave.GetMaxEvent())
        fMaxEvent = readerWave.GetMaxEvent();

    auto time_begin = std::chrono::system_clock::now();
    for (int i = 0; i < fMaxEvent; i++) {
        if (i > 0 && i % 10 == 0) {
            auto time_taken = std::chrono::system_clock::now() - time_begin;
            float percent_done = 1.0f * i / fMaxEvent;
            auto time_left = time_taken * (1.0f / percent_done - 1.0f);
            auto minutes_left = std::chrono::duration_cast<std::chrono::minutes>(time_left);
            auto seconds_left = std::chrono::duration_cast<std::chrono::seconds>(time_left - minutes_left);
            std::cout << "\r\033[F" << " " << i << " / " << fMaxEvent << " events  "
                      << minutes_left.count() << ":";
            printf("%02d left (%.1f %%) | ", (int)seconds_left.count(), percent_done * 100);
            GetFormattedRamInfo();
            std::cout << ANSI.END << std::endl;
        }
        TBevt<TBwaveform> aEvent;
        fPlotter.Fill(readerWave.GetAnEvent());
    }
    fPlotter.Update();
    fPlotter.SaveAs("DQM_Run" + std::to_string(fRunNum) + ".root");
}


template class TBmonit<TBwaveform>;
template class TBmonit<TBfastmode>;
