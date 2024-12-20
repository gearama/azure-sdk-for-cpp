# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

find_package(Git)

macro(GenerateCodeFromTypeSpec TSP_SHA TSP_REPO_PATH TSP_DESTINATION CODEGEN_SHA CODEGEN_DESTINATION  )

    message("Generating code using the following params TSP_SHA=${TSP_SHA} TSP_REPO_PATH=${TSP_REPO_PATH} TSP_DESTINATION=${TSP_DESTINATION} CODEGEN_SHA=${CODEGEN_SHA} CODEGEN_DESTINATION=${CODEGEN_DESTINATION}")
    set(CODEGEN_PATH packages/typespec-cpp)
    DownloadTSPFiles(${TSP_SHA} ${TSP_REPO_PATH}  ${TSP_DESTINATION})
    DownloadCodeGenerator(${CODEGEN_SHA} ${CODEGEN_DESTINATION})
    GenerateCodeFromTSP(${TSP_DESTINATION} ${TSP_REPO_PATH} ${CODEGEN_DESTINATION} ${CODEGEN_PATH})
endmacro()

macro(DownloadTSPFiles TSP_SHA TSP_REPO_PATH TSP_DESTINATION)
    message ("Downloading TSP files using the following params TSP_REPO=${TSP_REPO} TSP_SHA=${TSP_SHA} TSP_REPO_PATH=${TSP_REPO_PATH}  TSP_DESTINATION=${TSP_DESTINATION}")

    if(Git_FOUND)
        message("Git found: ${GIT_EXECUTABLE}")
    else()
        message(FATAL_ERROR "Git not found")
    endif()
    
    set(TSP_REPO "https://github.com/Azure/azure-rest-api-specs.git")
    set(DOWNLOAD_TSP_FOLDER ${CMAKE_SOURCE_DIR}/build/${TSP_DESTINATION})
    # if we have the git folder, we don't need to download it again
    # this also saves times on incremental builds
    if(NOT EXISTS ${DOWNLOAD_TSP_FOLDER}/.git)
    message("First time setting up the ${TSP_DESTINATION} repo.")
        #make folder
        make_directory(${DOWNLOAD_TSP_FOLDER})

    #init git in folder
    execute_process(COMMAND ${GIT_EXECUTABLE} init
            WORKING_DIRECTORY ${DOWNLOAD_TSP_FOLDER})
    #add remote
    execute_process(COMMAND ${GIT_EXECUTABLE} remote add origin ${TSP_REPO}
             WORKING_DIRECTORY ${DOWNLOAD_TSP_FOLDER})
    #set sparse-checkout
    execute_process(COMMAND ${GIT_EXECUTABLE} sparse-checkout init --cone
             WORKING_DIRECTORY ${DOWNLOAD_TSP_FOLDER})
    #set sparse-checkout folder
    execute_process(COMMAND ${GIT_EXECUTABLE} sparse-checkout set ${TSP_REPO_PATH}
             WORKING_DIRECTORY ${DOWNLOAD_TSP_FOLDER})
    else()
        message("Repo detected at ${TSP_DESTINATION}. Jumping ahead to checkout.") 
    endif()
    #fetch
    execute_process(COMMAND ${GIT_EXECUTABLE} fetch
         WORKING_DIRECTORY ${DOWNLOAD_TSP_FOLDER})
    #switch branch
    execute_process(COMMAND ${GIT_EXECUTABLE} checkout ${TSP_SHA}
          WORKING_DIRECTORY ${DOWNLOAD_TSP_FOLDER})
 
    if(NOT ${STATUS_CODE} EQUAL 0)
        message(FATAL_ERROR "TSP download failed.")
    endif()
endmacro()

macro (DownloadCodeGenerator CODEGEN_SHA CODEGEN_DESTINATION)
    message("Downloading CODEGEN files using the following params CODEGEN_REPO=${CODEGEN_REPO} CODEGEN_SHA=${CODEGEN_SHA} CODEGEN_DESTINATION=${CODEGEN_DESTINATION}")

    if(Git_FOUND)
        message("Git found: ${GIT_EXECUTABLE}")
    else()
        message(FATAL_ERROR "Git not found")
    endif()

    set(CODEGEN_REPO "https://github.com/Azure/autorest.cpp.git")
    set(DOWNLOAD_CODEGEN_FOLDER ${CMAKE_SOURCE_DIR}/build/${CODEGEN_DESTINATION})

    # if we have the git folder, we don't need to download it again
    # this also saves times on incremental builds
    if(NOT EXISTS ${DOWNLOAD_CODEGEN_FOLDER}/.git)
        message("First time setting up the ${CODEGEN_DESTINATION} repo.")
        #make folder
        make_directory(${DOWNLOAD_CODEGEN_FOLDER})
        #init git in folder
        execute_process(COMMAND ${GIT_EXECUTABLE} clone ${CODEGEN_REPO} ${DOWNLOAD_CODEGEN_FOLDER})
    else()
        message("Repo detected at  ${TSP_DESTINATION}. Jumping ahead to checkout.") 
    endif()

    #checkout SHA
    execute_process(COMMAND ${GIT_EXECUTABLE} checkout ${CODEGEN_SHA}
          WORKING_DIRECTORY ${DOWNLOAD_CODEGEN_FOLDER})
 
    if(NOT ${STATUS_CODE} EQUAL 0)
        message(FATAL_ERROR "CODEGEN download failed.")
    endif()
endmacro()

macro(GenerateCodeFromTSP TSP_DESTINATION TSP_REPO_PATH CODEGEN_DESTINATION CODEGEN_PATH)
    message("Generating code using the following params  TSP_DESTINATION=${TSP_DESTINATION} TSP_REPO_PATH=${TSP_REPO_PATH} CODEGEN_DESTINATION=${CODEGEN_DESTINATION}")
    message("Remember to Download the typspec-cpp emitter from npmjs.org")
    #TODO : https://github.com/Azure/azure-sdk-for-cpp/issues/6071
    set(DOWNLOAD_CODEGEN_FOLDER ${CMAKE_SOURCE_DIR}/build/${CODEGEN_DESTINATION}/${CODEGEN_PATH}/specs/)
    set(DOWNLOAD_TSP_FOLDER ${CMAKE_SOURCE_DIR}/build/${TSP_DESTINATION}/${TSP_REPO_PATH})
    set(SCRIPTS_FOLDER ${CMAKE_SOURCE_DIR}/eng/scripts/typespec/)
    message("Will copy tsp files from ${DOWNLOAD_TSP_FOLDER} to ${DOWNLOAD_CODEGEN_FOLDER}")
    #copy tsp files to the codegen folder
    file(COPY ${DOWNLOAD_TSP_FOLDER} 
    DESTINATION ${DOWNLOAD_CODEGEN_FOLDER})
    message("Will copy tsp generation scripts from ${SCRIPTS_FOLDER} to ${DOWNLOAD_CODEGEN_FOLDER}")
    file(COPY ${SCRIPTS_FOLDER}
    DESTINATION ${DOWNLOAD_CODEGEN_FOLDER})
    message("Will copy ${CMAKE_CURRENT_SOURCE_DIR}/client.tsp to ${DOWNLOAD_CODEGEN_FOLDER}")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/client.tsp
    DESTINATION ${DOWNLOAD_CODEGEN_FOLDER})
    message("Will copy ${CMAKE_CURRENT_SOURCE_DIR}/tspconfig.yaml to ${DOWNLOAD_CODEGEN_FOLDER}")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tspconfig.yaml
    DESTINATION ${DOWNLOAD_CODEGEN_FOLDER})
    #build codegen
    message("Building codegen in folder ${DOWNLOAD_CODEGEN_FOLDER}")
    execute_process(COMMAND pwsh Build-Codegen.ps1
    WORKING_DIRECTORY ${DOWNLOAD_CODEGEN_FOLDER})
    
    #generate code
    message("Use codegen in folder ${DOWNLOAD_CODEGEN_FOLDER}")
    execute_process(COMMAND pwsh Generate-Code.ps1
    WORKING_DIRECTORY ${DOWNLOAD_CODEGEN_FOLDER})
endmacro()

macro(UpdateCodeFilesFromGenerated CODEGEN_DESTINATION INCLUDE_DESTINATION SOURCE_DESTINATION)
    message("Updating code files using the following params CODEGEN_DESTINATION=${CODEGEN_DESTINATION} INCLUDE_DESTINATION=${INCLUDE_DESTINATION} SOURCE_DESTINATION=${SOURCE_DESTINATION}")
    set(CODEGEN_PATH packages/typespec-cpp/specs)
    set(INCLUDE_SRC ${CMAKE_SOURCE_DIR}/build/${CODEGEN_DESTINATION}/${CODEGEN_PATH}/generated/inc/)
    set(SOURCE_SRC ${CMAKE_SOURCE_DIR}/build/${CODEGEN_DESTINATION}/${CODEGEN_PATH}/generated/src/)

    message("Copying files from ${INCLUDE_SRC} to ${INCLUDE_DESTINATION}")
    file(COPY ${INCLUDE_SRC} DESTINATION ${INCLUDE_DESTINATION})

    message("Copying files from ${SOURCE_SRC} to ${SOURCE_DESTINATION}")
    file(COPY ${SOURCE_SRC} DESTINATION ${SOURCE_DESTINATION})
    message("CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
endmacro()
