#include <stdio.h>
#include <stdint.h>
#include <math.h>

#pragma pack(push, 1)
struct Particao
{
    uint8_t status;             // Byte 0: Status da particao
    uint8_t chs_inicial[3];     // Bytes 1-3: CHS inicial
    uint8_t tipo;               // Byte 4: Tipo de particao
    uint8_t chs_final[3];       // Bytes 5-7: CHS final
    uint32_t lba_inicial;       // Bytes 8-11: LBA do primeiro setor
    uint32_t contagem_setores;  // Bytes 12-15: Numero de setores
} typedef Particao;

struct MBR
{
    uint8_t bootstrap[440];               // Codigo bootstrap (os primeiros 440 bytes)
    uint32_t identificador_disco;         // Identificador do disco (bytes 440-443)
    uint16_t zeroed;                      // Zeros (bytes 444-445)
    Particao particoes[4];         // Quatro entradas de particao (64 bytes)
    uint16_t assinatura;                  // Assinatura 0x55AA
} typedef MBR;
#pragma pack(pop)

int chs_para_cilindro(uint8_t chs[3])
{
    return ((chs[1] << 2) | ((chs[0] & 0xC0) >> 6));
}

int chs_para_cabeca(uint8_t chs[3])
{
    return chs[2];
}

int chs_para_setor(uint8_t chs[3])
{
    return chs[0] & 0x3F;
}

double setores_para_gb(uint32_t setores)
{
    return (double)setores * 512 / pow(1024, 3);
}

void imprimir_info_particao(Particao *p, int index, int num_cabecas, int num_setores_por_trilha)
{
    char *tipo_particao;

    if (p->tipo == 0x00)
        return;
    else if (p->tipo == 0x83)
        tipo_particao = "Linux";
    else if (p->tipo == 0x82)
        tipo_particao = "Linux Swap";
    else if (p->tipo == 0x05)
        tipo_particao = "Estendida";
    else
        tipo_particao = "Desconhecido";

    printf("/dev/sda%-4d %s   %-6d %-4u %-10d %-2X %s\n",
           index + 1,
           p->status == 0x80 ? "*" : " ", // 0x80 = Ativa
           p->lba_inicial / (num_cabecas * num_setores_por_trilha),
           (p->lba_inicial + p->contagem_setores) / (num_cabecas * num_setores_por_trilha),
           p->contagem_setores / 2,
           p->tipo,
           tipo_particao);
}

void imprimir_dados_principais(MBR *m)
{
    float memoriaNoDisco = 0;

    for (int i = 0; i < 4; i++)
    {
        memoriaNoDisco += setores_para_gb(m->particoes[i].contagem_setores);
    }

    memoriaNoDisco = round(memoriaNoDisco);

    // Calculo dos cabecotes, setores/trilha, e cilindros reais a partir do CHS da primeira particao
    Particao *primeira_particao = &m->particoes[0];

    int cabecas = chs_para_cabeca(primeira_particao->chs_final);
    int setores_por_trilha = chs_para_setor(primeira_particao->chs_final);
    int cilindros = chs_para_cilindro(primeira_particao->chs_final);

    printf("Disco /dev/sda: %.f GB, %.f bytes, %.f setores\n", memoriaNoDisco, memoriaNoDisco * pow(1024, 3), (memoriaNoDisco * pow(1024, 3)) / 512);
    printf("%d cabecas, %d setores/trilha, %d cilindros\n", cabecas, setores_por_trilha, cilindros);
    printf("Unidades = cilindros de %d * 512 = %d bytes\n", setores_por_trilha * cabecas, setores_por_trilha * cabecas * 512);

    // Imprimir o Identificador do Disco extraido da MBR
    printf("Identificador do disco: 0x%08X\n\n", m->identificador_disco);
    printf("Dispositivo Boot Inicio Fim  Blocos     Id Sistema\n");

    for (int i = 0; i < 4; i++)
    {
        imprimir_info_particao(&m->particoes[i], i, cabecas, setores_por_trilha);
    }
}

int main()
{
    FILE *arquivo = fopen("mbr.bin", "rb");
    if (!arquivo)
    {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    MBR mbr;

    if (fread(&mbr, sizeof(MBR), 1, arquivo) != 1)
    {
        perror("Erro ao ler o arquivo");
        fclose(arquivo);
        return 1;
    }

    fclose(arquivo);

    if (mbr.assinatura != 0xAA55)
    {
        printf("Erro: Assinatura invalida, nao e uma MBR.\n");
        return 1;
    }

    imprimir_dados_principais(&mbr);

    return 0;
}
