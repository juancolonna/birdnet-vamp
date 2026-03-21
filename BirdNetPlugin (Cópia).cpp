#include "BirdNetPlugin.h"

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vamp/vamp.h>
#include <vamp-sdk/PluginAdapter.h>

using namespace Vamp;

// ── construtor / destrutor ───────────────────────────────────────────────────

BirdNetPlugin::BirdNetPlugin(float inputSampleRate)
    : Plugin(inputSampleRate)
    , m_blockSize(0)
    , m_threshold(0.1f)
    , m_topK(3)
    , m_stride(3.0f)
{
    const char* home = getenv("HOME");
    std::string homeDir = std::string(home ? home : "~");
    m_pythonPath = homeDir + "/miniconda3/envs/birdnet-plugin/bin/python3";
    m_scriptPath = homeDir + "/vamp/birdnet_run.py";
    m_logPath    = homeDir + "/vamp/birdnet_debug.log";
    m_wavPath    = homeDir + "/vamp/birdnet_analise.wav";
}

BirdNetPlugin::~BirdNetPlugin() {}

// ── inicialização ────────────────────────────────────────────────────────────

bool BirdNetPlugin::initialise(size_t, size_t, size_t blockSize) {
    m_blockSize = (int)blockSize;
    m_audioBuffer.clear();

    std::ofstream log(m_logPath);
    log << "initialise() chamado, blockSize=" << m_blockSize << "\n";
    log << "scriptPath=" << m_scriptPath << "\n";
    log << "wavPath=" << m_wavPath << "\n";

    return true;
}

void BirdNetPlugin::reset() {
    m_audioBuffer.clear();
}

// ── acumulação de amostras ───────────────────────────────────────────────────

Plugin::FeatureSet
BirdNetPlugin::process(const float* const* inputBuffers,
                       Vamp::RealTime /*timestamp*/)
{
    for (int i = 0; i < m_blockSize; i++)
        m_audioBuffer.push_back(inputBuffers[0][i]);

    return FeatureSet();
}

// ── análise completa ao final ────────────────────────────────────────────────

Plugin::FeatureSet BirdNetPlugin::getRemainingFeatures() {
    std::ofstream log(m_logPath, std::ios::app);
    log << "getRemainingFeatures() chamado, buffer: "
        << m_audioBuffer.size() << " amostras\n";

    FeatureSet output;

    if (m_audioBuffer.empty()) {
        log << "ERRO: buffer vazio\n";
        return output;
    }

    escreverWAV(m_wavPath,
                m_audioBuffer.data(),
                (int)m_audioBuffer.size(),
                (int)m_inputSampleRate);
    m_audioBuffer.clear();
    log << "WAV salvo em: " << m_wavPath << "\n";

    std::ostringstream cmd;
    cmd << m_pythonPath << " " << m_scriptPath
        << " " << m_wavPath
        << " >> " << m_logPath << " 2>&1";
    log << "Executando: " << cmd.str() << "\n";
    log.flush();

    // executa e captura stdout separadamente
    std::ostringstream cmd2;
    cmd2 << m_pythonPath << " " << m_scriptPath
         << " " << m_wavPath
         << " " << m_threshold
         << " " << m_topK
         << " " << m_stride
         << " 2>>" << m_logPath;

    FILE* pipe = popen(cmd2.str().c_str(), "r");
    if (!pipe) {
        log << "ERRO: popen falhou\n";
        return output;
    }

    std::string saida;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe))
        saida += buf;
    pclose(pipe);

    log << "Saida recebida: " << saida.size() << " bytes\n";
    log << "Saida: " << saida << "\n";

    for (auto& d : parseJSON(saida)) {
        Feature f;
        f.hasTimestamp = true;
        f.timestamp    = RealTime::fromSeconds(d.time_s);
        f.hasDuration  = true;
        f.duration     = RealTime::fromSeconds(3.0);
        f.label        = d.species +
                         " (" +
                         std::to_string((int)std::round(d.confidence * 100)) +
                         "%)";
        f.values.push_back(d.confidence);
        output[0].push_back(f);
    }

    log << "Labels geradas: " << output[0].size() << "\n";

    std::remove(m_wavPath.c_str());
    log << "WAV temporario removido.\n";

    return output;
}

// ── WAV writer (PCM 16-bit mono) ─────────────────────────────────────────────

void BirdNetPlugin::escreverWAV(const std::string& path,
                                 const float* samples,
                                 int n, int sr) const
{
    std::ofstream f(path, std::ios::binary);

    auto w16 = [&](uint16_t v){ f.write((char*)&v, 2); };
    auto w32 = [&](uint32_t v){ f.write((char*)&v, 4); };

    uint32_t dataBytes = (uint32_t)(n * 2);
    f.write("RIFF", 4); w32(36 + dataBytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4); w32(16);
    w16(1);                          // PCM
    w16(1);                          // mono
    w32((uint32_t)sr);               // sample rate
    w32((uint32_t)(sr * 2));         // byte rate
    w16(2);                          // block align
    w16(16);                         // bits per sample
    f.write("data", 4); w32(dataBytes);

    for (int i = 0; i < n; i++) {
        float   v = std::max(-1.0f, std::min(1.0f, samples[i]));
        int16_t s = (int16_t)(v * 32767.0f);
        f.write((char*)&s, 2);
    }
}

// ── parser JSON minimalista ──────────────────────────────────────────────────

std::vector<BirdNetPlugin::Deteccao>
BirdNetPlugin::parseJSON(const std::string& json) const
{
    std::vector<Deteccao> lista;
    size_t pos = 0;

    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;

        std::string obj = json.substr(pos, end - pos + 1);

        auto str = [&](const std::string& key) -> std::string {
            auto k = obj.find("\"" + key + "\"");
            if (k == std::string::npos) return "";
            auto c  = obj.find(':', k);
            auto q1 = obj.find('"', c + 1);
            auto q2 = obj.find('"', q1 + 1);
            return obj.substr(q1 + 1, q2 - q1 - 1);
        };
        auto num = [&](const std::string& key) -> float {
            auto k = obj.find("\"" + key + "\"");
            if (k == std::string::npos) return 0.f;
            auto c = obj.find(':', k);
            return std::strtof(obj.c_str() + c + 1, nullptr);
        };

        Deteccao d;
        d.species    = str("species");
        d.confidence = num("confidence");
        d.time_s     = num("time_s");

        if (!d.species.empty())
            lista.push_back(d);

        pos = end + 1;
    }
    return lista;
}

// ── tamanho de bloco preferido ───────────────────────────────────────────────

size_t BirdNetPlugin::getPreferredBlockSize() const { return 1024; }
size_t BirdNetPlugin::getPreferredStepSize()  const { return 1024; }


// ── parâmetros configuráveis ─────────────────────────────────────────────────

Plugin::ParameterList BirdNetPlugin::getParameterDescriptors() const {
    ParameterDescriptor p;
    p.identifier   = "threshold";
    p.name         = "Confidence Threshold";
    p.description  = "Limiar minimo de confianca para deteccao";
    p.unit         = "";
    p.minValue     = 0.0f;
    p.maxValue     = 1.0f;
    p.defaultValue = 0.1f;
    p.isQuantized  = false;
    ParameterDescriptor p2;
    p2.identifier   = "top_k";
    p2.name         = "Top K Species";
    p2.description  = "Numero maximo de especies por segmento";
    p2.unit         = "";
    p2.minValue     = 1.0f;
    p2.maxValue     = 10.0f;
    p2.defaultValue = 3.0f;
    p2.isQuantized  = true;
    p2.quantizeStep = 1.0f;
    ParameterDescriptor p3;
    p3.identifier   = "stride";
    p3.name         = "Stride (s)";
    p3.description  = "Passo da janela deslizante em segundos";
    p3.unit         = "s";
    p3.minValue     = 1.0f;
    p3.maxValue     = 3.0f;
    p3.defaultValue = 3.0f;
    p3.isQuantized  = false;
    return { p, p2, p3 };
}

float BirdNetPlugin::getParameter(std::string id) const {
    if (id == "threshold") return m_threshold;
    if (id == "top_k")     return (float)m_topK;
    if (id == "stride")    return m_stride;
    return 0.0f;
}

void BirdNetPlugin::setParameter(std::string id, float value) {
    if (id == "threshold") m_threshold = value;
    if (id == "top_k")     m_topK = (int)value;
    if (id == "stride")    m_stride = value;
}




std::string BirdNetPlugin::getIdentifier()    const { return "birdnet-vamp"; }
std::string BirdNetPlugin::getName()          const { return "BirdNET"; }
std::string BirdNetPlugin::getDescription()   const { return "Deteccao de aves com BirdNET"; }
std::string BirdNetPlugin::getMaker()         const { return "Prof. Dr. Juan G. Colonna <github.com/juancolonna>"; }
std::string BirdNetPlugin::getCopyright()     const { return "MIT"; }
int         BirdNetPlugin::getPluginVersion() const { return 1; }

Plugin::InputDomain BirdNetPlugin::getInputDomain() const {
    return TimeDomain;
}

Plugin::OutputList BirdNetPlugin::getOutputDescriptors() const {
    OutputDescriptor d;
    d.identifier       = "deteccoes";
    d.name             = "Deteccoes BirdNET";
    d.description      = "Especie detectada com confianca e timestamp";
    d.unit             = "";
    d.hasFixedBinCount = true;
    d.binCount         = 1;
    d.sampleType       = OutputDescriptor::VariableSampleRate;
    d.hasDuration      = true;
    return { d };
}

// ── entry point ──────────────────────────────────────────────────────────────

const VampPluginDescriptor*
vampGetPluginDescriptor(unsigned int version, unsigned int index) {
    if (version < 1 || index > 0) return nullptr;
    static Vamp::PluginAdapter<BirdNetPlugin> adapter;
    return adapter.getDescriptor();
}
