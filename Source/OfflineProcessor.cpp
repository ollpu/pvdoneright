#include "OfflineProcessor.h"

#include <climits>
#include <iostream>

#include "ScalingAudioSource.h"

bool processWavFile(File inputPath, File outputPath, double scalingRatio, PlayerMode mode)
{
    if (!outputPath.hasFileExtension("wav"))
    {
        std::cerr << "Only WAV output is supported" << std::endl;
        return false;
    }

    const double maxRatio = 10.;
    if (!std::isfinite(scalingRatio) || scalingRatio < 1 / maxRatio || scalingRatio > maxRatio)
    {
        std::cerr << "Scaling ratio out of range" << std::endl;
        return false;
    }

    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(inputPath));

    if (!reader.get())
    {
        std::cerr << "Failed to open input file" << std::endl;
        return false;
    }

    AudioFormatReaderSource formatReaderSource(reader.get(), false);
    ResamplingAudioSource pre_ressource(&formatReaderSource, false);
    ScalingAudioSource ssource(&pre_ressource, false, reader->numChannels, maxRatio);
    ResamplingAudioSource post_ressource(&ssource, false);

    const int blockSize = 4096;
    post_ressource.prepareToPlay(blockSize, reader->sampleRate);

    int64 resultLength = reader->lengthInSamples;
    if (mode == PlayerMode::PITCH_SHIFTING)
    {
        pre_ressource.setResamplingRatio(jmax(1.0, scalingRatio));
        post_ressource.setResamplingRatio(jmin(1.0, scalingRatio));
        ssource.setScalingRatio(1.0 / scalingRatio);
    }
    else if (mode == PlayerMode::TIME_SCALING)
    {
        pre_ressource.setResamplingRatio(1.0);
        post_ressource.setResamplingRatio(1.0);
        ssource.setScalingRatio(scalingRatio);
        resultLength /= scalingRatio;
    }
    else if (mode == PlayerMode::TIME_PITCH_SCALING)
    {
        pre_ressource.setResamplingRatio(jmax(1.0, scalingRatio));
        post_ressource.setResamplingRatio(jmin(1.0, scalingRatio));
        ssource.setScalingRatio(1.0);
        resultLength /= scalingRatio;
    }

    auto stream = std::make_unique<FileOutputStream>(outputPath);
    if (stream->failedToOpen())
    {
        std::cerr << "Failed to open output file" << std::endl;
        return false;
    }
    stream->setPosition(0);
    stream->truncate();
    WavAudioFormat outputFormat;
    std::unique_ptr<AudioFormatWriter> writer(outputFormat.createWriterFor(stream.get(), reader->sampleRate, reader->numChannels, reader->bitsPerSample, {}, 0));

    if (!writer.get())
    {
        std::cerr << "Failed to initialize output writer" << std::endl;
        return false;
    }
    stream.release();

    // Discard one frame of samples at the beginning, pad extra at end
    // This is not exact (and is hardcoded)
    {
        AudioBuffer<float> tmp(reader->numChannels, 4096);
        post_ressource.getNextAudioBlock(AudioSourceChannelInfo(tmp));
    }
    resultLength += 2 * 4096;
    if (resultLength > INT_MAX)
    {
        std::cerr << "Output length too long" << std::endl;
        return false;
    }

    bool ok = writer->writeFromAudioSource(post_ressource, int(resultLength), blockSize);
    if (!ok)
    {
        std::cerr << "Error while writing output file" << std::endl;
        return false;
    }

    return true;
}
