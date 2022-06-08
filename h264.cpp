extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
}
#include<stdio.h>
#include<stdlib.h>
#include<iostream>

#define INBUF_SIZE 4096

int SavePicture(AVFrame* pFrame, char* out_name) {
    int width = pFrame->width;
    int height = pFrame->height;
    AVCodecContext *pCodeCtx = 0;
   
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    // 设置输出文件格式
    pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);
    // 创建并初始化AVIOContext
    if (avio_open(&pFormatCtx->pb, out_name, AVIO_FLAG_READ_WRITE) < 0) {
        std::cout << "Could not open output file" << std::endl;;
        return -1;
    }
    // create stream
    AVStream *pAVStream = avformat_new_stream(pFormatCtx, 0);
    if (pAVStream == NULL) {
        return -1;
    }
    AVCodecParameters *parameters = pAVStream->codecpar;
    parameters->codec_id = pFormatCtx->oformat->video_codec;
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->format = AV_PIX_FMT_YUV420P;
    parameters->width = pFrame->width;
    parameters->height = pFrame->height;
    
    std::cout << "编码器id: " << pAVStream->codecpar->codec_type << std::endl;
    std::cout << AV_CODEC_ID_MJPEG << std::endl;
    AVCodec *pCodec = const_cast<AVCodec *>(avcodec_find_encoder(pAVStream->codecpar->codec_id));
    if (!pCodec){ 
        std::cout << "Could not find encoder" << std::endl;
        return -1;
    }
    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        exit(1);
    }
    if ((avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }
    std::cout << "111111111" << std::endl;
    pCodeCtx->time_base = (AVRational){1, 25};
    if (avcodec_open2(pCodeCtx, pCodec, NULL) < 0) {
        std::cout << "Could not open codec" << std::endl;
        return -1;
    }
    int ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        std::cout << "Write_header fail\n";
        return -1;
    } 

    int y_size = width * height;

    AVPacket pkt;
    av_new_packet(&pkt, y_size*3);   
 
    // 编码数据
    ret = avcodec_receive_packet(pCodeCtx, &pkt);
    if (ret < 0) {
        std::cout << "Could not avcodec_receive_packet\n";
        return -1;
    }
    ret = av_write_frame(pFormatCtx, &pkt);
    if (ret < 0) {
        std::cout << "Could not av_write_frame\n";
        return -1;
    }
    av_packet_unref(&pkt);
    av_write_trailer(pFormatCtx);
    avcodec_close(pCodeCtx);
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
    return 0;
}


int decode(AVCodecContext *avctx, AVFrame *frame, AVPacket *pkt, int frame_count) {
    int ret = 0;
    char buf[1024];
    const char* out_filename = "./picture";
    ret = avcodec_send_packet(avctx, pkt);
    if (ret < 0) {
        std::cout << "error sending a packet for decoding" << std::endl;
        return -1;
    }
    /*
    AVERROR(EAGAIN)表示输入的packet未被接收
    */
    while (ret >= 0) {
        ret = avcodec_receive_frame(avctx, frame);
        std::cout << "encode_ret = " << ret << std::endl;
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            std::cout << "AVERROR(EAGAIN) or AVERROR_EOF" << std::endl;
            return 0;
        }
        else if (ret < 0) {
            return -2;
        }
        std::cout << "解码器id: " << avctx->codec_id << std::endl;
        snprintf(buf, sizeof(buf), "%s/picture-%d.jpg", out_filename, frame_count);
        SavePicture(frame, buf);
    }
    return 0;
}

int main() {
    /*输入和输出文件*/
    FILE* inp_file;
    FILE* out_file;

    int ret = 0;

    AVCodec *codec;
    AVCodecContext *avctx;
    AVPacket *pkt;
    AVFrame *frame;
    AVCodecParserContext * parser;

    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t data_size;

    int frame_count = 0;

    inp_file = fopen("tmp.h264", "rb");
    out_file = fopen("test.yuv", "wb+");

    int nalLen = 0;

    //分配内存
    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    std::cout << "开始" << std::endl;

    // 1、 查找 H264 CODEC
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cout << "codec not found" << std::endl;
        return -1;
    }

    // 2、 初始化parser
    parser = av_parser_init(codec->id);
    if (!parser) {
        std::cout << "parser not found" << std::endl;
        return -2;
    }

    // 3、 分配context
    avctx = avcodec_alloc_context3(codec);
    if (!avctx) {
        std::cout << "could not allocate ctx" << std::endl;
        return -3;
    }


    // 4、打开解码器
    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        std::cout << "could not open codec, ret=" << ret << std::endl;
        return -4;
    }
    
    std::cout << "开始解码" << std::endl;
    // 5、 开始解码
    std::cout << feof(inp_file) << std::endl;
    while (!feof(inp_file)) {
        data_size = fread(inbuf, 1, INBUF_SIZE, inp_file);
        if (data_size <= 0) break;
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, avctx, &pkt->data, &pkt->size, data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            // std::cout << "ret = " << ret << std::endl;
            data += ret;
            data_size -= ret;

            // std::cout << "pkt_size=" << pkt->size << std::endl;
            if (pkt->size){
                decode(avctx, frame, pkt, frame_count);
                frame_count++;
            }
        }
    }
    decode(avctx, frame, pkt, frame_count);

    fclose(inp_file);
    fclose(out_file);

    av_parser_close(parser);
    avcodec_free_context(&avctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return 0;
}

