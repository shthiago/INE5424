# README

A seguir serão descritos os objetivos identificados, pontos implementados e problemas encontrados no processo de desenvolvimento do P3;

## OBJETIVOS E PLANO DE IMPLEMENTAÇÃO

O primeiro objetivo identificado foi a necessidade de implementar métodos na CPU do ARMv7 capazes gerar uma syscall e chamar um handler inicial para tratamento da interrupção; esses dois métodos foram implementados em `armv7_cpu_syscalled.cc` e `armv1_cpu_syscall.cc`. O conteúdo dessas implementações será discutido mais adiante;

Além dos meios de gerar e direcionar o tratamento de uma syscall também foi necessária a adição de uma referência ao código do handler inicial no EPOS; isso foi adicionado na função `software_interrupt()` do `InterruptHandler` do EPOS. Essa referência se encontra no arquivo `raspberry_pi3_ic.cc`.

Com os métodos necessários para gerar um interrupção e salvar o contexto de execução definidos o próximo passo foi a definição de um handler inicial para o tratamento da interrupção gerada. O handler definido será apresentado mais a frente. Após a definição inicial do handler uma classe `SyscallMessage` foi definida com o intuito de armazenar informações referentes ao tipo de interrupção gerada.

Por fim, a classe responsável por conter os códigos de execução de cada syscall foi a agent.h; o objetivo dessa classe era o de conter o código de todas as syscalls que o grupo conseguisse implementar.

Concluindo os objetivos e plano de implementação, apesar de não conseguir implementar todas as syscalls esperadas, seguindo o nosso plano de trabalho fomos capazes de gerar e executar uma syscall, ainda que simples.

Além disso o binding do código de print que gera uma syscall foi realizado, de maneira a garantir que todos os prints chamados gerem uma syscall

Ao executar a aplicação hello é possível ver a execução de uma aplicação que realiza uma syscall.

## PROBLEMAS ENCONTRADOS

Durante a tentativa de implementação da syscall que finaliza uma Thread foi identificado um problema que acreditamos ser de ligação de arquivos. O seguinte erro de compilação ocorre `Undefined reference to EPOS::S::Thread::exit(int)`, entretanto nos testes aplicados, a classe Thread é referenciada normalmente por qualquer outra parte do EPOS. Também tivemos dificuldade com a compilação do nosso código quando tentávamos implementar as syscall de outras classes do EPOS, dado que o código as vezes compilava e executava, e outras não.


## PARTES IMPLEMENTADAS

Utilizamos a implementação já presente na branch, `IC::software_interrup()` implementada em `raspberry_pi_ic.cc`, modificando-a para que esta chamasse a `CPU::syscalled()`. Dessa forma, ao invocar a instrução SVC, a função `syscalled` é chamada. Para fazer a comunicação entre os modos, a função `syscall` recebe um void pointer, que definimos que seria um ponteiro para a classe `SyscallMessage` implementada em `message.h`. Essa classe funciona apenas como um envelope, que é criado pelo chamador da `syscall`, colocada no r0 por esta para que quando `syscall` execute `SVC` a função `syscalled` possa pegar o ponteiro do r0, fazer o cast para `SyscallMessage` e instancia um `Handler` fazer o tratamento da interrupção. O `Handler` faz um `switch` sobre o tipo da syscall, e para cada caso, chama a função correspondente de `Agent`, que executa a syscall em si. Dessa forma, para implementar a syscall de print, alteramos a função `_print` no arquivo `system_scaffold.cc` para construir uma mensagem com a `s` recebida e passar um poteiro para essa para a função `CPU::syscall`. Abaixo os arquivos relevantes na nossa implementação:

### `handler.h`

Lida com o `SyscallMessage`, chamando a função do agente referente ao tipo da mensagem recebida, por exemplo, se o tipo da syscall for `PRINT` `agent.print()` será chamado.

### `agent.h`
Objeto responsável por conter a implementação do código de alguma syscall. Recebe uma `SyscallMessage` como parâmetro e usa as informações dela para executar a syscall específica solicitada.


### `message.h`

Objeto responsável por armazenar as informações de uma mensagem syscall, os atributos presentes são `_type`, `_text` e `_integer`.


### `armv7_cpu_syscalled.cc`
    
Implementamos a interpretação da mensagem gerada na syscall quando a interrupção era gerada. Antes de fazer o tratamento da chamada, empilhamos os registradores r1-r21, e após o retorno do tratamento, desempilhamos. Essa função faz o cast do ponteiro para um ponteiro para a mensagem e cria um handler que recebe a mensagem.

### `armv7_cpu_syscall.cc`

Implementamos a chamada da instrução SVC efetivamente, fazendo push do registrador LR, chamando a instrução SVC e retornando o registrador LR.

## O QUE FARIAMOS SE TIVESSEMOS MAIS TEMPO

Considerando que conseguimos fazer a estrutura básica para o uso de uma syscall, que funcionou para print, os próximos passos que teríamos dado seriam o de implementar os códigos de syscall referentes às classes do EPOS, as quais tivemos dificuldade de acessar de dentro da classe `agent.h`. Por fim, após conseguir implementar todas as syscalls para essas classes do EPOS dentro da classe `Agent` realizaríamos o biding dos códigos que as executam, da mesma maneira feita com a syscall print que implementamos.
