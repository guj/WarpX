/* Copyright 2024 The WarpX Community
 *
 * This file is part of WarpX.
 *
 * Authors: Roelof Groenewald (TAE Technologies)
 *
 * License: BSD-3-Clause-LBNL
 */
#include "DSMCFunc.H"
#include "Utils/TextMsg.H"

/**
 * \brief Constructor of the DSMCFunc class
 *
 * @param[in] collision_name the name of the collision
 * @param[in] mypc pointer to the MultiParticleContainer
 * @param[in] isSameSpecies whether the two colliding species are the same
 */
DSMCFunc::DSMCFunc (
    const std::string& collision_name,
    [[maybe_unused]] MultiParticleContainer const * const mypc,
    const bool isSameSpecies ): m_isSameSpecies{isSameSpecies}
{
    using namespace amrex::literals;

    const amrex::ParmParse pp_collision_name(collision_name);

    // query for a list of collision processes
    // these could be elastic, excitation, charge_exchange, back, etc.
    amrex::Vector<std::string> scattering_process_names;
    pp_collision_name.queryarr("scattering_processes", scattering_process_names);

    // create a vector of ScatteringProcess objects from each scattering
    // process name
    bool ionization_flag = false;
    for (const auto& scattering_process : scattering_process_names) {
        const std::string kw_cross_section = scattering_process + "_cross_section";
        std::string cross_section_file;
        pp_collision_name.query(kw_cross_section.c_str(), cross_section_file);

        // if the scattering process is excitation, ionization or forward get
        // the energy associated with that process
        // (note that this allows forward scattering to be used both with and
        // without a fixed energy loss)
        amrex::ParticleReal energy = 0._prt;
        if (scattering_process.find("excitation") != std::string::npos ||
            scattering_process.find("ionization") != std::string::npos ||
            scattering_process.find("forward") != std::string::npos ) {
            const std::string kw_energy = scattering_process + "_energy";
            utils::parser::getWithParser(
                pp_collision_name, kw_energy.c_str(), energy);
        }

        ScatteringProcess process(scattering_process, cross_section_file, energy);

        WARPX_ALWAYS_ASSERT_WITH_MESSAGE(process.type() != ScatteringProcessType::INVALID,
                                        "Cannot add an unknown scattering process type");

        // Only one ionization process is currently supported as part of a given
        // collision set.
        if (process.type() == ScatteringProcessType::IONIZATION) {
            WARPX_ALWAYS_ASSERT_WITH_MESSAGE(
                !ionization_flag,
                "DSMC only supports a single ionization process"
            );
            ionization_flag = true;

            // And add a check that the ionization species has the same mass
            // (and a positive charge), compared to the target species
        }
        m_scattering_processes.push_back(std::move(process));
    }

    // Store ScatteringProcess::Executor(s).
#ifdef AMREX_USE_GPU
    amrex::Gpu::HostVector<ScatteringProcess::Executor> h_scattering_processes_exe;
    for (auto const& p : m_scattering_processes) {
        h_scattering_processes_exe.push_back(p.executor());
    }
    m_scattering_processes_exe.resize(h_scattering_processes_exe.size());
    amrex::Gpu::copyAsync(amrex::Gpu::hostToDevice, h_scattering_processes_exe.begin(),
                          h_scattering_processes_exe.end(), m_scattering_processes_exe.begin());
    amrex::Gpu::streamSynchronize();
#else
    for (auto const& p : m_scattering_processes) {
        m_scattering_processes_exe.push_back(p.executor());
    }
#endif

    // Link executor to appropriate ScatteringProcess executors
    m_exe.m_scattering_processes_data = m_scattering_processes_exe.data();
    m_exe.m_process_count = static_cast<int>(m_scattering_processes_exe.size());
    m_exe.m_isSameSpecies = m_isSameSpecies;
}
