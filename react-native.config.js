module.exports = {
  dependency: {
    platforms: {
      android: {
        sourceDir: './android',
        packageImportPath: 'import com.op.sqlite.OPSQLitePackage;',
        buildTypes: []
      },
      ios: {
        podspecPath: './op-sqlite.podspec'
      },
      windows: {
        sourceDir: './windows',
        solutionFile: 'OPSQLite.sln',
        projects: [{
          projectFile: 'OPSQLite\\OPSQLite.vcxproj',
          directDependency: true
        }]
      }
    }
  }
};
