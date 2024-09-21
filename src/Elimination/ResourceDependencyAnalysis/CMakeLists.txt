# add_library( LLVMResourceDependencyAnalysis MODULE ResourceDependencyAnalysis.cpp )
# target_compile_options(LLVMResourceDependencyAnalysis PUBLIC -fno-rtti)
# set_target_properties(LLVMResourceDependencyAnalysis PROPERTIES OUTPUT_NAME ResourceDependencyAnalysis)

# add_library( LLVMgetOverallStruct MODULE getOverallStruct.cpp )
# target_compile_options(LLVMgetOverallStruct PUBLIC -fno-rtti)
# set_target_properties(LLVMgetOverallStruct PROPERTIES OUTPUT_NAME libgetOverallStruct)

add_executable( WizardMerge main.cpp ModifiedFileInVariant.cpp Utils.cpp DiffTextSplit.cpp GraphShrinking.cpp 
        GraphGeneration/FunctionNode.cpp GraphGeneration/GlobalVariableNode.cpp GraphGeneration/ModuleSummary.cpp
        GraphGeneration/TypeNode.cpp GraphGeneration/Utils.cpp)
# target_compile_options(WizardMerge PUBLIC -fno-rtti -fsanitize=address)
target_compile_options(WizardMerge PUBLIC -fno-rtti)
target_compile_features(WizardMerge PRIVATE cxx_std_17)
set_target_properties(WizardMerge PROPERTIES OUTPUT_NAME WizardMerge)
set(LLVM_LINK_COMPONENTS Analysis)
llvm_map_components_to_libnames(llvm_libs support core irreader Analysis transformutils)

target_link_options(WizardMerge PUBLIC -fsanitize=address)
target_link_libraries(WizardMerge ${llvm_libs})
message(STATUS "Libs: ${llvm_libs}")
