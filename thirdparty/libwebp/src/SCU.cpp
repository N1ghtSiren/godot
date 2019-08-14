// Single Compilation Unit
#define SCU_IDENT(x) x
#define SCU_XSTR(x) #x
#define SCU_STR(x) SCU_XSTR(x)
#define SCU_PATH(x,y) SCU_STR(SCU_IDENT(x)SCU_IDENT(y))
#define SCU_DIR 

#include SCU_PATH(SCU_DIR,dec/alpha_dec.c)
#include SCU_PATH(SCU_DIR,dec/buffer_dec.c)
#include SCU_PATH(SCU_DIR,dec/frame_dec.c)
#include SCU_PATH(SCU_DIR,dec/idec_dec.c)
#include SCU_PATH(SCU_DIR,dec/io_dec.c)
#include SCU_PATH(SCU_DIR,dec/quant_dec.c)
#include SCU_PATH(SCU_DIR,dec/tree_dec.c)
#include SCU_PATH(SCU_DIR,dec/vp8_dec.c)
#include SCU_PATH(SCU_DIR,dec/vp8l_dec.c)
#include SCU_PATH(SCU_DIR,dec/webp_dec.c)
#include SCU_PATH(SCU_DIR,demux/anim_decode.c)
#include SCU_PATH(SCU_DIR,demux/demux.c)
#include SCU_PATH(SCU_DIR,dsp/alpha_processing.c)
#include SCU_PATH(SCU_DIR,dsp/alpha_processing_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/alpha_processing_neon.c)
#include SCU_PATH(SCU_DIR,dsp/alpha_processing_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/alpha_processing_sse41.c)
#include SCU_PATH(SCU_DIR,dsp/cost.c)
#include SCU_PATH(SCU_DIR,dsp/cost_mips32.c)
#include SCU_PATH(SCU_DIR,dsp/cost_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/cost_neon.c)
#include SCU_PATH(SCU_DIR,dsp/cost_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/cpu.c)
#include SCU_PATH(SCU_DIR,dsp/dec.c)
#include SCU_PATH(SCU_DIR,dsp/dec_clip_tables.c)
#include SCU_PATH(SCU_DIR,dsp/dec_mips32.c)
#include SCU_PATH(SCU_DIR,dsp/dec_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/dec_msa.c)
#include SCU_PATH(SCU_DIR,dsp/dec_neon.c)
#include SCU_PATH(SCU_DIR,dsp/dec_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/dec_sse41.c)
#include SCU_PATH(SCU_DIR,dsp/enc.c)
#include SCU_PATH(SCU_DIR,dsp/enc_mips32.c)
#include SCU_PATH(SCU_DIR,dsp/enc_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/enc_msa.c)
#include SCU_PATH(SCU_DIR,dsp/enc_neon.c)
#include SCU_PATH(SCU_DIR,dsp/enc_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/enc_sse41.c)
#include SCU_PATH(SCU_DIR,dsp/filters.c)
#include SCU_PATH(SCU_DIR,dsp/filters_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/filters_msa.c)
#include SCU_PATH(SCU_DIR,dsp/filters_neon.c)
#include SCU_PATH(SCU_DIR,dsp/filters_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/lossless.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc_mips32.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc_msa.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc_neon.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_enc_sse41.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_msa.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_neon.c)
#include SCU_PATH(SCU_DIR,dsp/lossless_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/rescaler.c)
#include SCU_PATH(SCU_DIR,dsp/rescaler_mips32.c)
#include SCU_PATH(SCU_DIR,dsp/rescaler_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/rescaler_msa.c)
#include SCU_PATH(SCU_DIR,dsp/rescaler_neon.c)
#include SCU_PATH(SCU_DIR,dsp/rescaler_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/ssim.c)
#include SCU_PATH(SCU_DIR,dsp/ssim_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/upsampling.c)
#include SCU_PATH(SCU_DIR,dsp/upsampling_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/upsampling_msa.c)
#include SCU_PATH(SCU_DIR,dsp/upsampling_neon.c)
#include SCU_PATH(SCU_DIR,dsp/upsampling_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/upsampling_sse41.c)
#include SCU_PATH(SCU_DIR,dsp/yuv.c)
#include SCU_PATH(SCU_DIR,dsp/yuv_mips32.c)
#include SCU_PATH(SCU_DIR,dsp/yuv_mips_dsp_r2.c)
#include SCU_PATH(SCU_DIR,dsp/yuv_neon.c)
#include SCU_PATH(SCU_DIR,dsp/yuv_sse2.c)
#include SCU_PATH(SCU_DIR,dsp/yuv_sse41.c)
#include SCU_PATH(SCU_DIR,enc/alpha_enc.c)
#include SCU_PATH(SCU_DIR,enc/analysis_enc.c)
#include SCU_PATH(SCU_DIR,enc/backward_references_cost_enc.c)
#include SCU_PATH(SCU_DIR,enc/backward_references_enc.c)
#include SCU_PATH(SCU_DIR,enc/config_enc.c)
#include SCU_PATH(SCU_DIR,enc/cost_enc.c)
#include SCU_PATH(SCU_DIR,enc/filter_enc.c)
#include SCU_PATH(SCU_DIR,enc/frame_enc.c)
#include SCU_PATH(SCU_DIR,enc/histogram_enc.c)
#include SCU_PATH(SCU_DIR,enc/iterator_enc.c)
#include SCU_PATH(SCU_DIR,enc/near_lossless_enc.c)
#include SCU_PATH(SCU_DIR,enc/picture_csp_enc.c)
#include SCU_PATH(SCU_DIR,enc/picture_enc.c)
#include SCU_PATH(SCU_DIR,enc/picture_psnr_enc.c)
#include SCU_PATH(SCU_DIR,enc/picture_rescale_enc.c)
#include SCU_PATH(SCU_DIR,enc/picture_tools_enc.c)
#include SCU_PATH(SCU_DIR,enc/predictor_enc.c)
#include SCU_PATH(SCU_DIR,enc/quant_enc.c)
#include SCU_PATH(SCU_DIR,enc/syntax_enc.c)
#include SCU_PATH(SCU_DIR,enc/token_enc.c)
#include SCU_PATH(SCU_DIR,enc/tree_enc.c)
#include SCU_PATH(SCU_DIR,enc/vp8l_enc.c)
#include SCU_PATH(SCU_DIR,enc/webp_enc.c)
#include SCU_PATH(SCU_DIR,mux/anim_encode.c)
#include SCU_PATH(SCU_DIR,mux/muxedit.c)
#include SCU_PATH(SCU_DIR,mux/muxinternal.c)
#include SCU_PATH(SCU_DIR,mux/muxread.c)
#include SCU_PATH(SCU_DIR,utils/bit_reader_utils.c)
#include SCU_PATH(SCU_DIR,utils/bit_writer_utils.c)
#include SCU_PATH(SCU_DIR,utils/color_cache_utils.c)
#include SCU_PATH(SCU_DIR,utils/filters_utils.c)
#include SCU_PATH(SCU_DIR,utils/huffman_encode_utils.c)
#include SCU_PATH(SCU_DIR,utils/huffman_utils.c)
#include SCU_PATH(SCU_DIR,utils/quant_levels_dec_utils.c)
#include SCU_PATH(SCU_DIR,utils/quant_levels_utils.c)
#include SCU_PATH(SCU_DIR,utils/random_utils.c)
#include SCU_PATH(SCU_DIR,utils/rescaler_utils.c)
#include SCU_PATH(SCU_DIR,utils/thread_utils.c)
#include SCU_PATH(SCU_DIR,utils/utils.c)
