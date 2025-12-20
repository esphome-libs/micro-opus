# cmake/sources.cmake
# Source file definitions for microOpus
# Separated from main CMakeLists.txt for maintainability
#
# All opus source paths use ${OPUS_DIR} which should be set before including
# this file. It points to either:
#   - lib/opus (original submodule, for reference)
#   - ${CMAKE_CURRENT_BINARY_DIR}/opus-staged (patched staging directory)

# Guard against multiple inclusion
if(__opus_sources_defined)
    return()
endif()
set(__opus_sources_defined TRUE)

# ==============================================================================
# opus_get_sources
# ==============================================================================
# Populates source file lists using the specified opus directory.
# Call this after setting up the staged directory.
#
# Arguments:
#   OPUS_DIR - Path to the opus directory (staged or original)
# ==============================================================================
function(opus_get_sources OPUS_DIR)
    # --------------------------------------------------------------------------
    # Core Opus sources
    # --------------------------------------------------------------------------
    set(OPUS_BASE_SOURCES
        ${OPUS_DIR}/src/opus.c
        ${OPUS_DIR}/src/repacketizer.c
        ${OPUS_DIR}/src/mapping_matrix.c
        PARENT_SCOPE
    )

    set(OPUS_DECODER_SOURCES
        ${OPUS_DIR}/src/opus_decoder.c
        ${OPUS_DIR}/src/opus_multistream_decoder.c
        ${OPUS_DIR}/src/opus_projection_decoder.c
        ${OPUS_DIR}/src/extensions.c
        PARENT_SCOPE
    )

    set(OPUS_ENCODER_SOURCES
        ${OPUS_DIR}/src/opus_encoder.c
        ${OPUS_DIR}/src/opus_multistream_encoder.c
        ${OPUS_DIR}/src/opus_projection_encoder.c
        ${OPUS_DIR}/src/opus_multistream.c
        ${OPUS_DIR}/src/analysis.c
        ${OPUS_DIR}/src/mlp.c
        ${OPUS_DIR}/src/mlp_data.c
        PARENT_SCOPE
    )

    # --------------------------------------------------------------------------
    # CELT sources
    # --------------------------------------------------------------------------
    set(CELT_SOURCES
        ${OPUS_DIR}/celt/bands.c
        ${OPUS_DIR}/celt/celt.c
        ${OPUS_DIR}/celt/cwrs.c
        ${OPUS_DIR}/celt/entcode.c
        ${OPUS_DIR}/celt/entdec.c
        ${OPUS_DIR}/celt/kiss_fft.c
        ${OPUS_DIR}/celt/laplace.c
        ${OPUS_DIR}/celt/mathops.c
        ${OPUS_DIR}/celt/mdct.c
        ${OPUS_DIR}/celt/modes.c
        ${OPUS_DIR}/celt/pitch.c
        ${OPUS_DIR}/celt/celt_lpc.c
        ${OPUS_DIR}/celt/quant_bands.c
        ${OPUS_DIR}/celt/rate.c
        ${OPUS_DIR}/celt/vq.c
        ${OPUS_DIR}/celt/celt_decoder.c
        ${OPUS_DIR}/celt/celt_encoder.c
        ${OPUS_DIR}/celt/entenc.c
        PARENT_SCOPE
    )

    # --------------------------------------------------------------------------
    # SILK base sources (shared between fixed and float)
    # --------------------------------------------------------------------------
    set(SILK_BASE_SOURCES
        ${OPUS_DIR}/silk/CNG.c
        ${OPUS_DIR}/silk/code_signs.c
        ${OPUS_DIR}/silk/gain_quant.c
        ${OPUS_DIR}/silk/interpolate.c
        ${OPUS_DIR}/silk/LP_variable_cutoff.c
        ${OPUS_DIR}/silk/NLSF_stabilize.c
        ${OPUS_DIR}/silk/NLSF_VQ_weights_laroia.c
        ${OPUS_DIR}/silk/pitch_est_tables.c
        ${OPUS_DIR}/silk/resampler.c
        ${OPUS_DIR}/silk/resampler_down2_3.c
        ${OPUS_DIR}/silk/resampler_down2.c
        ${OPUS_DIR}/silk/resampler_private_AR2.c
        ${OPUS_DIR}/silk/resampler_private_down_FIR.c
        ${OPUS_DIR}/silk/resampler_private_IIR_FIR.c
        ${OPUS_DIR}/silk/resampler_private_up2_HQ.c
        ${OPUS_DIR}/silk/resampler_rom.c
        ${OPUS_DIR}/silk/shell_coder.c
        ${OPUS_DIR}/silk/sigm_Q15.c
        ${OPUS_DIR}/silk/sort.c
        ${OPUS_DIR}/silk/sum_sqr_shift.c
        ${OPUS_DIR}/silk/tables_gain.c
        ${OPUS_DIR}/silk/tables_LTP.c
        ${OPUS_DIR}/silk/tables_NLSF_CB_NB_MB.c
        ${OPUS_DIR}/silk/tables_NLSF_CB_WB.c
        ${OPUS_DIR}/silk/tables_other.c
        ${OPUS_DIR}/silk/tables_pitch_lag.c
        ${OPUS_DIR}/silk/tables_pulses_per_block.c
        ${OPUS_DIR}/silk/VAD.c
        ${OPUS_DIR}/silk/control_audio_bandwidth.c
        ${OPUS_DIR}/silk/quant_LTP_gains.c
        ${OPUS_DIR}/silk/VQ_WMat_EC.c
        ${OPUS_DIR}/silk/HP_variable_cutoff.c
        ${OPUS_DIR}/silk/NLSF_unpack.c
        ${OPUS_DIR}/silk/NLSF_del_dec_quant.c
        ${OPUS_DIR}/silk/process_NLSFs.c
        ${OPUS_DIR}/silk/stereo_LR_to_MS.c
        ${OPUS_DIR}/silk/stereo_MS_to_LR.c
        ${OPUS_DIR}/silk/check_control_input.c
        ${OPUS_DIR}/silk/control_SNR.c
        ${OPUS_DIR}/silk/control_codec.c
        ${OPUS_DIR}/silk/A2NLSF.c
        ${OPUS_DIR}/silk/ana_filt_bank_1.c
        ${OPUS_DIR}/silk/biquad_alt.c
        ${OPUS_DIR}/silk/bwexpander_32.c
        ${OPUS_DIR}/silk/bwexpander.c
        ${OPUS_DIR}/silk/debug.c
        ${OPUS_DIR}/silk/decode_pitch.c
        ${OPUS_DIR}/silk/inner_prod_aligned.c
        ${OPUS_DIR}/silk/lin2log.c
        ${OPUS_DIR}/silk/log2lin.c
        ${OPUS_DIR}/silk/LPC_analysis_filter.c
        ${OPUS_DIR}/silk/LPC_inv_pred_gain.c
        ${OPUS_DIR}/silk/table_LSF_cos.c
        ${OPUS_DIR}/silk/NLSF2A.c
        ${OPUS_DIR}/silk/stereo_decode_pred.c
        ${OPUS_DIR}/silk/stereo_encode_pred.c
        ${OPUS_DIR}/silk/stereo_find_predictor.c
        ${OPUS_DIR}/silk/stereo_quant_pred.c
        ${OPUS_DIR}/silk/LPC_fit.c
        ${OPUS_DIR}/silk/init_decoder.c
        ${OPUS_DIR}/silk/decode_core.c
        ${OPUS_DIR}/silk/decode_frame.c
        ${OPUS_DIR}/silk/decode_parameters.c
        ${OPUS_DIR}/silk/decode_indices.c
        ${OPUS_DIR}/silk/decode_pulses.c
        ${OPUS_DIR}/silk/decoder_set_fs.c
        ${OPUS_DIR}/silk/dec_API.c
        ${OPUS_DIR}/silk/NLSF_decode.c
        ${OPUS_DIR}/silk/PLC.c
        ${OPUS_DIR}/silk/enc_API.c
        ${OPUS_DIR}/silk/encode_indices.c
        ${OPUS_DIR}/silk/encode_pulses.c
        ${OPUS_DIR}/silk/NSQ.c
        ${OPUS_DIR}/silk/NSQ_del_dec.c
        ${OPUS_DIR}/silk/NLSF_encode.c
        ${OPUS_DIR}/silk/NLSF_VQ.c
        ${OPUS_DIR}/silk/init_encoder.c
        PARENT_SCOPE
    )

    # --------------------------------------------------------------------------
    # SILK fixed-point sources
    # --------------------------------------------------------------------------
    set(SILK_FIXED_SOURCES
        ${OPUS_DIR}/silk/fixed/LTP_analysis_filter_FIX.c
        ${OPUS_DIR}/silk/fixed/LTP_scale_ctrl_FIX.c
        ${OPUS_DIR}/silk/fixed/corrMatrix_FIX.c
        ${OPUS_DIR}/silk/fixed/encode_frame_FIX.c
        ${OPUS_DIR}/silk/fixed/find_LPC_FIX.c
        ${OPUS_DIR}/silk/fixed/find_LTP_FIX.c
        ${OPUS_DIR}/silk/fixed/find_pitch_lags_FIX.c
        ${OPUS_DIR}/silk/fixed/find_pred_coefs_FIX.c
        ${OPUS_DIR}/silk/fixed/noise_shape_analysis_FIX.c
        ${OPUS_DIR}/silk/fixed/process_gains_FIX.c
        ${OPUS_DIR}/silk/fixed/regularize_correlations_FIX.c
        ${OPUS_DIR}/silk/fixed/residual_energy16_FIX.c
        ${OPUS_DIR}/silk/fixed/residual_energy_FIX.c
        ${OPUS_DIR}/silk/fixed/warped_autocorrelation_FIX.c
        ${OPUS_DIR}/silk/fixed/apply_sine_window_FIX.c
        ${OPUS_DIR}/silk/fixed/autocorr_FIX.c
        ${OPUS_DIR}/silk/fixed/burg_modified_FIX.c
        ${OPUS_DIR}/silk/fixed/k2a_FIX.c
        ${OPUS_DIR}/silk/fixed/k2a_Q16_FIX.c
        ${OPUS_DIR}/silk/fixed/pitch_analysis_core_FIX.c
        ${OPUS_DIR}/silk/fixed/vector_ops_FIX.c
        ${OPUS_DIR}/silk/fixed/schur64_FIX.c
        ${OPUS_DIR}/silk/fixed/schur_FIX.c
        PARENT_SCOPE
    )

    # --------------------------------------------------------------------------
    # SILK floating-point sources
    # --------------------------------------------------------------------------
    set(SILK_FLOAT_SOURCES
        ${OPUS_DIR}/silk/float/apply_sine_window_FLP.c
        ${OPUS_DIR}/silk/float/corrMatrix_FLP.c
        ${OPUS_DIR}/silk/float/encode_frame_FLP.c
        ${OPUS_DIR}/silk/float/find_LPC_FLP.c
        ${OPUS_DIR}/silk/float/find_LTP_FLP.c
        ${OPUS_DIR}/silk/float/find_pitch_lags_FLP.c
        ${OPUS_DIR}/silk/float/find_pred_coefs_FLP.c
        ${OPUS_DIR}/silk/float/LPC_analysis_filter_FLP.c
        ${OPUS_DIR}/silk/float/LTP_analysis_filter_FLP.c
        ${OPUS_DIR}/silk/float/LTP_scale_ctrl_FLP.c
        ${OPUS_DIR}/silk/float/noise_shape_analysis_FLP.c
        ${OPUS_DIR}/silk/float/process_gains_FLP.c
        ${OPUS_DIR}/silk/float/regularize_correlations_FLP.c
        ${OPUS_DIR}/silk/float/residual_energy_FLP.c
        ${OPUS_DIR}/silk/float/warped_autocorrelation_FLP.c
        ${OPUS_DIR}/silk/float/wrappers_FLP.c
        ${OPUS_DIR}/silk/float/autocorrelation_FLP.c
        ${OPUS_DIR}/silk/float/burg_modified_FLP.c
        ${OPUS_DIR}/silk/float/bwexpander_FLP.c
        ${OPUS_DIR}/silk/float/energy_FLP.c
        ${OPUS_DIR}/silk/float/inner_product_FLP.c
        ${OPUS_DIR}/silk/float/k2a_FLP.c
        ${OPUS_DIR}/silk/float/LPC_inv_pred_gain_FLP.c
        ${OPUS_DIR}/silk/float/pitch_analysis_core_FLP.c
        ${OPUS_DIR}/silk/float/scale_copy_vector_FLP.c
        ${OPUS_DIR}/silk/float/scale_vector_FLP.c
        ${OPUS_DIR}/silk/float/schur_FLP.c
        ${OPUS_DIR}/silk/float/sort_FLP.c
        PARENT_SCOPE
    )

    # --------------------------------------------------------------------------
    # ESP32-S3 Xtensa LX7 optimization sources
    # --------------------------------------------------------------------------
    set(XTENSA_LX7_SOURCES
        ${OPUS_DIR}/celt/xtensa/mathops_lx7.c
        PARENT_SCOPE
    )
endfunction()

# ==============================================================================
# Non-opus sources (these don't depend on OPUS_DIR)
# ==============================================================================

# Ogg Opus decoder (C++ wrapper) - in our src/ directory
set(OGG_OPUS_SOURCES
    src/opus_header.cpp
    src/ogg_opus_decoder.cpp
)

# Thread-local storage sources (for THREADSAFE_PSEUDOSTACK mode)
set(THREAD_LOCAL_SOURCES
    patches/thread_local_stack.c
)
